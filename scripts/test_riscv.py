#!/usr/bin/env python3
"""
RISC-V hypervisor QEMU test runner.

Usage:
    python3 scripts/test_riscv.py
    python3 scripts/test_riscv.py --mode boot --runs 10
    python3 scripts/test_riscv.py --mode stress --smp 1 --runs 5 -v
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
BOOT_TIMEOUT = 30
EXIT_TIMEOUT = 5

CPP_SMOKE = "modern cpp smoke: 42"
CPP_CTOR = "modern cpp global ctor: 1"
CPP_RAI = "modern cpp raii lock: 99"
RISCV_BOOT = "HyperCoreRT RISC-V booting"
GUEST_HELLO = "hello,guest"

FATAL_PATTERNS = (
    "Kernel panic",
)

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
    ci_dir = script.parents[1]
    project_root = ci_dir.parent

    qemu_candidates = [
        project_root / "qemu-11.0" / "build" / "qemu-system-riscv64",
        Path("/usr/bin/qemu-system-riscv64"),
    ]
    qemu_default = next(
        (str(p) for p in qemu_candidates if p.exists()), "qemu-system-riscv64")

    parser = argparse.ArgumentParser(
        description="RISC-V hypervisor QEMU test runner.")
    parser.add_argument("--mode", choices=["boot", "stress"],
                        default="boot",
                        help="Test mode: boot (boot only), stress (boot+repeated)")
    parser.add_argument("--runs", type=int, default=1,
                        help="Number of test runs.")
    parser.add_argument("--stop-on-fail", action="store_true",
                        help="Stop after first failure.")
    parser.add_argument("--log-dir", type=str, default="/tmp/hyper_riscv_test",
                        help="Directory for per-run logs.")
    parser.add_argument("--smp", type=int, default=1,
                        help="Number of physical CPUs.")
    parser.add_argument("--memory", default="512",
                        help="QEMU memory size in MB.")
    parser.add_argument("--hyper-bin", type=existing_path,
                        default=ci_dir / "bazel-bin" / "hyper-elf")
    parser.add_argument("--guest-bin", type=str, default=None,
                        help="Path to guest binary (ELF or raw). "
                        "Default: auto-detect from bazel-bin.")
    parser.add_argument("--qemu", default=qemu_default)
    parser.add_argument("--boot-timeout", type=int, default=BOOT_TIMEOUT)
    parser.add_argument("-v", "--verbose", action="store_true")
    return parser.parse_args()


def resolve_guest_bin(args, ci_dir):
    """Resolve guest binary path — use --guest-bin or auto-detect."""
    if args.guest_bin:
        path = Path(args.guest_bin).expanduser().resolve()
        if not path.exists():
            raise FileNotFoundError(f"guest binary not found: {path}")
        return path
    # Auto-detect: prefer ELF (can be loaded directly by QEMU generic loader)
    candidates = [
        ci_dir / "bazel-bin" / "test" / "arch" / "riscv64" / "guest-elf",
        ci_dir / "bazel-bin" / "test" / "arch" / "riscv64" / "guest.bin",
    ]
    for c in candidates:
        if c.exists():
            return c
    raise FileNotFoundError(
        f"guest binary not found, tried: {[str(c) for c in candidates]}")


def build_qemu_cmd(args, guest_bin):
    cmd = [
        args.qemu,
        "-M", "virt",
        "-nographic",
        "-bios", "default",
        "-smp", str(args.smp),
        "-m", str(args.memory),
        "-no-reboot",
        "-kernel", str(args.hyper_bin),
    ]
    # Load guest binary at 0x90080000 via QEMU generic loader
    if guest_bin.suffix == ".bin":
        cmd += ["-device", f"loader,file={guest_bin},addr=0x90080000"]
    else:
        # ELF: QEMU loads at ELF-specified addresses
        cmd += ["-device", f"loader,file={guest_bin}"]
    return cmd


def terminate_qemu(proc):
    if proc.poll() is not None:
        return
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

        for needle in needles:
            if needle in text:
                return text

    raise TimeoutError(f"timed out waiting for: {needles}")


# ---------------------------------------------------------------------------
# Test modes
# ---------------------------------------------------------------------------

def get_text(output_parts):
    return "".join(output_parts)


def test_boot(args, proc, output_parts):
    """Wait for hypervisor boot markers and guest output."""
    read_until(proc, [CPP_SMOKE], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [CPP_CTOR], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [CPP_RAI], args.boot_timeout, output_parts, args.verbose)

    # Wait for RISC-V boot + sstatus print (indicates basic HW init passed)
    read_until(proc, ["sstatus:"], args.boot_timeout, output_parts, args.verbose)

    # Wait for guest to print "hello,guest"
    read_until(proc, [GUEST_HELLO], args.boot_timeout, output_parts, args.verbose)

    # Give it a moment to finish remaining output
    time.sleep(1)

    text = get_text(output_parts)
    if RISCV_BOOT not in text:
        raise AssertionError("RISC-V boot marker not found")


def test_stress(args, proc, output_parts):
    """Boot test — same as boot for now, will add more checks later."""
    test_boot(args, proc, output_parts)


TEST_FUNCS = {
    "boot": test_boot,
    "stress": test_stress,
}


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

def run_single(args, run_idx, log_dir, guest_bin):
    cmd = build_qemu_cmd(args, guest_bin)
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
    ci_dir = Path(__file__).resolve().parents[1]
    log_dir = Path(args.log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)

    guest_bin = resolve_guest_bin(args, ci_dir)
    print(f"Guest: {guest_bin}")

    print(f"Arch: riscv64  Mode: {args.mode}  SMP: {args.smp}  Runs: {args.runs}")
    print(f"QEMU: {args.qemu}")
    print(f"Logs: {log_dir}")
    print()

    results = []
    pass_count = 0
    fail_count = 0

    for i in range(1, args.runs + 1):
        ts = datetime.now().strftime("%H:%M:%S")
        label = f"[{ts}] Run {i}/{args.runs}"
        print(f"{label} ... ", end="", flush=True)

        result = run_single(args, i, log_dir, guest_bin)
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
    print(f"Arch=riscv64 Mode={args.mode} SMP={args.smp}: "
          f"{pass_count}/{args.runs} passed")
    print("=" * 60)

    if fail_count > 0:
        sys.exit(1)

    print("\nAll runs passed!")
    sys.exit(0)


if __name__ == "__main__":
    main()
