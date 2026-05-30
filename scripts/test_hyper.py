#!/usr/bin/env python3
"""
Unified HyperCoreRT QEMU test runner.

Usage:
    python3 test_hyper.py --smp 2 --mode stress --runs 10
    python3 test_hyper.py --smp 1 --mode boot
    python3 test_hyper.py --smp 2 --mode vuart
"""

import argparse
import os
import select
import signal
import subprocess
import sys
import time
from pathlib import Path
from datetime import datetime

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
BOOT_TIMEOUT = 120
COMMAND_TIMEOUT = 20
EXIT_TIMEOUT = 10

PASS_MARKER = "[stress] ALL PASS"
FAIL_MARKER = "[stress] SOME FAILURES"
PANIC_MARKER = "Kernel panic"
READY_MARKER = "HyperCoreRT + Linux SMP - Ready"
PROMPT = "~ #"
CPP_SMOKE = "modern cpp smoke: 42"
CPP_CTOR = "modern cpp global ctor: 1"
CPP_RAI = "modern cpp raii lock: 99"

FATAL_PATTERNS = (
    "Kernel panic",
    "Attempted to kill init",
    "unsupport irq",
    "invalid ipa",
    "guest exception failed",
    "do_data_abort failed",
    "hyper_sync",
)

SMP_CHECK_PATTERNS = {
    1: ["SMP: Total of 1 processor"],
    2: ["SMP: Total of 2 processors", "CPU1: Booted secondary"],
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def existing_path(value):
    path = Path(value).expanduser().resolve()
    if not path.exists():
        raise argparse.ArgumentTypeError(f"path does not exist: {path}")
    return path


def parse_args():
    script = Path(__file__).resolve()
    ci_dir = script.parents[1]          # .../h_ci
    project_root = ci_dir.parent        # .../hyper_core

    parser = argparse.ArgumentParser(
        description="Unified HyperCoreRT QEMU test runner.")
    parser.add_argument("--mode", choices=["boot", "stress", "vuart"],
                        default="stress",
                        help="Test mode: boot (boot only), stress (boot+stress), vuart (boot+vuart)")
    parser.add_argument("--runs", type=int, default=1,
                        help="Number of test runs (stress mode).")
    parser.add_argument("--stop-on-fail", action="store_true",
                        help="Stop after first failure.")
    parser.add_argument("--log-dir", type=str, default="/tmp/hyper_test",
                        help="Directory for per-run logs.")
    parser.add_argument("--smp", type=int, default=1,
                        help="Number of physical CPUs.")
    parser.add_argument("--memory", default="6000",
                        help="QEMU memory size.")
    parser.add_argument("--hyper-bin", type=existing_path,
                        default=ci_dir / "bazel-bin" / "core.bin")
    parser.add_argument("--hyper-dtb", type=existing_path,
                        default=ci_dir / "bazel-bin" / "hyper.dtb")
    parser.add_argument("--image", type=existing_path,
                        default=project_root / "Image")
    parser.add_argument("--linux-dtb", type=existing_path,
                        default=project_root / "linux.dtb")
    parser.add_argument("--rootfs", type=existing_path,
                        default=project_root / "rootfs.img")
    parser.add_argument("--qemu", default="qemu-system-aarch64")
    parser.add_argument("--boot-timeout", type=int, default=BOOT_TIMEOUT)
    parser.add_argument("--command-timeout", type=int, default=COMMAND_TIMEOUT)
    parser.add_argument("--input-delay", type=float, default=0.01)
    parser.add_argument("-v", "--verbose", action="store_true")
    return parser.parse_args()


def build_qemu_cmd(args):
    return [
        args.qemu,
        "-M", "virt,virtualization=true,gic-version=3,secure=on",
        "-cpu", "cortex-a57",
        "-nographic",
        "-smp", str(args.smp),
        "-m", str(args.memory),
        "-no-reboot",
        "-kernel", str(args.hyper_bin),
        "-dtb", str(args.hyper_dtb),
        "-device", f"loader,file={args.image},force-raw=on,addr=0x50200000",
        "-device", f"loader,file={args.linux_dtb},force-raw=on,addr=0x65000000",
        "-device", f"loader,file={args.rootfs},force-raw=on,addr=0x66000000",
    ]


def terminate_qemu(proc):
    if proc.poll() is not None:
        return
    try:
        proc.stdin.write("\x01x")
        proc.stdin.flush()
        proc.wait(timeout=EXIT_TIMEOUT)
        return
    except Exception:
        pass
    try:
        os.killpg(proc.pid, signal.SIGTERM)
        proc.wait(timeout=EXIT_TIMEOUT)
        return
    except Exception:
        pass
    try:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.wait(timeout=EXIT_TIMEOUT)
    except Exception:
        pass


def read_until(proc, needles, timeout, output_buf, verbose=False):
    """Read QEMU output until one of *needles* is found or timeout."""
    deadline = time.monotonic() + timeout
    text = ""
    needle_list = list(needles)

    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"QEMU exited early with code {proc.returncode}")

        ready, _, _ = select.select([proc.stdout], [], [], 0.2)
        if not ready:
            continue

        chunk = os.read(proc.stdout.fileno(), 8192).decode("utf-8", errors="replace")
        if not chunk:
            continue

        output_buf.append(chunk)
        text += chunk

        if verbose:
            sys.stdout.write(chunk)
            sys.stdout.flush()

        for fatal in FATAL_PATTERNS:
            if fatal in text:
                raise RuntimeError(f"fatal pattern: {fatal}")

        for needle in needle_list:
            if needle in text:
                return text

    raise TimeoutError(f"timed out waiting for: {needle_list}")


def send_line(proc, line, delay):
    for ch in line + "\n":
        proc.stdin.write(ch)
        proc.stdin.flush()
        if delay > 0:
            time.sleep(delay)


# ---------------------------------------------------------------------------
# Test modes
# ---------------------------------------------------------------------------

def get_text(output_parts):
    return "".join(output_parts)


def test_boot(args, proc, output_parts):
    """Wait for hypervisor + Linux boot to complete."""
    read_until(proc, [CPP_SMOKE], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [CPP_CTOR], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [CPP_RAI], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [READY_MARKER], args.boot_timeout, output_parts, args.verbose)

    text = get_text(output_parts)

    # SMP validation
    expected = SMP_CHECK_PATTERNS.get(args.smp, [])
    for pat in expected:
        if pat not in text:
            raise AssertionError(f"SMP check failed: '{pat}' not found in output")

    # pCPU bring-up checks (only for SMP > 1)
    if args.smp > 1:
        if "pcpu1 online" not in text:
            raise AssertionError("pcpu1 did not come online")
        if "switch on pCPU1" not in text:
            raise AssertionError("No context switch on pCPU1")


def test_stress(args, proc, output_parts):
    """Boot + wait for in-guest stress test."""
    test_boot(args, proc, output_parts)
    text = read_until(proc, [PASS_MARKER, FAIL_MARKER],
                      args.boot_timeout, output_parts, args.verbose)
    if FAIL_MARKER in text:
        raise AssertionError("In-guest stress test reported failures")
    if PASS_MARKER not in text:
        raise AssertionError("Stress test result marker not found")


def test_vuart(args, proc, output_parts):
    """Boot + vUART interactive test."""
    test_boot(args, proc, output_parts)
    read_until(proc, [PROMPT], args.command_timeout, output_parts, args.verbose)

    time.sleep(2)
    begin = "__VUART_LS_BEGIN__"
    end = "__VUART_LS_END__"
    send_line(proc, f"echo {begin}; ls /; echo {end}", args.input_delay)
    output = read_until(proc, [end], args.command_timeout, output_parts, args.verbose)

    expected = ("bin", "dev", "init", "proc", "sys", "usr")
    missing = [e for e in expected if e not in output]
    if missing:
        raise AssertionError(f"ls / missing entries: {missing}")


TEST_FUNCS = {
    "boot": test_boot,
    "stress": test_stress,
    "vuart": test_vuart,
}


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

def run_single(args, run_idx, log_dir):
    cmd = build_qemu_cmd(args)
    result = {
        "run": run_idx,
        "status": "unknown",
        "error": None,
        "duration": 0.0,
        "log_file": None,
    }

    log_file = log_dir / f"run_{run_idx:03d}.log"
    result["log_file"] = str(log_file)

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
        preexec_fn=os.setsid,
    )

    output_parts = []
    start = time.monotonic()

    try:
        test_fn = TEST_FUNCS[args.mode]
        test_fn(args, proc, output_parts)
        result["status"] = "PASS"

    except (TimeoutError, RuntimeError, AssertionError) as e:
        if result["status"] == "unknown":
            result["status"] = "FAIL"
        result["error"] = str(e)
    except Exception as e:
        result["status"] = "ERROR"
        result["error"] = str(e)
    finally:
        result["duration"] = time.monotonic() - start
        terminate_qemu(proc)

        with open(log_file, "w") as f:
            f.write(f"Run: {run_idx}\n")
            f.write(f"Mode: {args.mode}  SMP: {args.smp}\n")
            f.write(f"Status: {result['status']}\n")
            f.write(f"Duration: {result['duration']:.1f}s\n")
            if result["error"]:
                f.write(f"Error: {result['error']}\n")
            f.write(f"Command: {' '.join(cmd)}\n")
            f.write("=" * 80 + "\n")
            f.write(get_text(output_parts))

    return result


def main():
    args = parse_args()
    log_dir = Path(args.log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)

    print(f"Mode: {args.mode}  SMP: {args.smp}  Runs: {args.runs}")
    print(f"Logs: {log_dir}")
    print()

    results = []
    pass_count = 0
    fail_count = 0

    for i in range(1, args.runs + 1):
        ts = datetime.now().strftime("%H:%M:%S")
        label = f"[{ts}] Run {i}/{args.runs}"
        print(f"{label} ... ", end="", flush=True)

        result = run_single(args, i, log_dir)
        results.append(result)

        if result["status"] == "PASS":
            pass_count += 1
            print(f"PASS ({result['duration']:.1f}s)")
        else:
            fail_count += 1
            print(f"{result['status']} ({result['duration']:.1f}s) - {result['error']}")
            if args.stop_on_fail:
                break

    print()
    print("=" * 60)
    print(f"Mode={args.mode} SMP={args.smp}: "
          f"{pass_count}/{args.runs} passed")
    print("=" * 60)

    if fail_count > 0:
        sys.exit(1)

    print("\nAll runs passed!")
    sys.exit(0)


if __name__ == "__main__":
    main()
