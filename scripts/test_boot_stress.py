#!/usr/bin/env python3
"""
Boot stress test for HyperCoreRT.

Runs QEMU with an in-guest SMP stress test (4 parallel workers inside init).
No UART input needed — the stress test runs autonomously inside the guest.

Usage:
    python3 test_boot_stress.py --runs 20
    python3 test_boot_stress.py --runs 20 --stop-on-fail
"""

import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path
from datetime import datetime

BOOT_TIMEOUT = 120
EXIT_TIMEOUT = 10

PASS_MARKER = "[stress] ALL PASS"
FAIL_MARKER = "[stress] SOME FAILURES"
PANIC_MARKER = "Kernel panic"

FATAL_PATTERNS = (
    "Kernel panic",
    "Attempted to kill init",
    "unsupport irq",
    "invalid ipa",
    "guest exception failed",
    "do_data_abort failed",
    "hyper_sync",
)


def existing_path(value):
    path = Path(value).expanduser().resolve()
    if not path.exists():
        raise argparse.ArgumentTypeError(f"path does not exist: {path}")
    return path


def parse_args():
    test_path = Path(__file__).resolve()
    hyper_dir = test_path.parents[2]
    project_root = hyper_dir.parent

    parser = argparse.ArgumentParser(description="Boot stress test for HyperCoreRT.")
    parser.add_argument("--runs", type=int, default=10, help="Number of test runs.")
    parser.add_argument("--stop-on-fail", action="store_true",
                        help="Stop immediately after the first failure.")
    parser.add_argument("--log-dir", type=str, default="/tmp/hyper_stress",
                        help="Directory to store per-run logs.")
    parser.add_argument("--hyper-elf", type=existing_path,
                        default=hyper_dir / "bazel-bin" / "hyper-elf")
    parser.add_argument("--hyper-dtb", type=existing_path,
                        default=hyper_dir / "bazel-bin" / "hyper.dtb")
    parser.add_argument("--image", type=existing_path,
                        default=project_root / "Image")
    parser.add_argument("--linux-dtb", type=existing_path,
                        default=project_root / "linux.dtb")
    parser.add_argument("--rootfs", type=existing_path,
                        default=project_root / "rootfs.img")
    parser.add_argument("--qemu", default="qemu-system-aarch64")
    parser.add_argument("--smp", type=int, default=1)
    parser.add_argument("--memory", default="6000")
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
        "-kernel", str(args.hyper_elf),
        "-append", "nokaslr",
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


def run_single_test(args, run_idx, log_dir):
    """Run a single boot+stress test. Returns result dict."""
    cmd = build_qemu_cmd(args)
    result = {
        "run": run_idx,
        "status": "unknown",
        "phase": "unknown",
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

    start_time = time.monotonic()
    output = b""

    try:
        result["phase"] = "boot"

        deadline = time.monotonic() + BOOT_TIMEOUT
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                result["phase"] = "qemu_exit"
                raise RuntimeError(
                    f"QEMU exited early with code {proc.returncode}")

            chunk = os.read(proc.stdout.fileno(), 8192)
            if not chunk:
                time.sleep(0.1)
                continue

            output += chunk
            text = output.decode("utf-8", errors="replace")

            if PASS_MARKER in text:
                result["status"] = "PASS"
                break

            if FAIL_MARKER in text:
                # Extract round failures
                result["status"] = "STRESS-FAIL"
                for line in text.splitlines():
                    if "FAIL" in line and "round" in line:
                        result["error"] = line.strip()
                        break
                break

            if PANIC_MARKER in text:
                result["phase"] = "kernel_panic"
                for line in text.splitlines():
                    if "Kernel panic" in line:
                        result["error"] = line.strip()
                        break
                raise RuntimeError(result["error"] or "kernel panic")

        else:
            result["status"] = "TIMEOUT"
            result["error"] = (
                f"no [{PASS_MARKER}] or [{FAIL_MARKER}] within "
                f"{BOOT_TIMEOUT}s")

    except RuntimeError as e:
        if result["status"] == "unknown":
            result["status"] = "FATAL"
            result["error"] = str(e)
    except Exception as e:
        result["status"] = "ERROR"
        result["error"] = str(e)
    finally:
        result["duration"] = time.monotonic() - start_time
        terminate_qemu(proc)

        with open(log_file, "wb") as f:
            f.write(f"Run: {run_idx}\n".encode())
            f.write(f"Status: {result['status']}\n".encode())
            f.write(f"Phase: {result['phase']}\n".encode())
            f.write(f"Duration: {result['duration']:.1f}s\n".encode())
            if result["error"]:
                f.write(f"Error: {result['error']}\n".encode())
            f.write(f"Command: {' '.join(cmd)}\n".encode())
            f.write(b"=" * 80 + b"\n")
            f.write(output)

    return result


def analyze_failures(results):
    """Print analysis of failure patterns."""
    failures = [r for r in results if r["status"] != "PASS"]
    if not failures:
        return

    print("\n" + "=" * 80)
    print("FAILURE ANALYSIS")
    print("=" * 80)

    by_status = {}
    for f in failures:
        s = f["status"]
        by_status.setdefault(s, []).append(f)

    for status, fs in sorted(by_status.items()):
        print(f"\n{status} ({len(fs)} failures):")
        for f in fs:
            print(f"  Run {f['run']}: {f['duration']:.1f}s - {f['error']}")

    print(f"\nDetailed logs in: {results[0]['log_file'].rsplit('/', 1)[0]}")


def main():
    args = parse_args()
    log_dir = Path(args.log_dir)
    log_dir.mkdir(parents=True, exist_ok=True)

    print(f"Running {args.runs} boot stress tests...")
    print(f"Logs: {log_dir}")
    print()

    results = []
    pass_count = 0
    fail_count = 0

    for i in range(1, args.runs + 1):
        ts = datetime.now().strftime("%H:%M:%S")
        print(f"[{ts}] Run {i}/{args.runs} ... ", end="", flush=True)

        result = run_single_test(args, i, log_dir)
        results.append(result)

        if result["status"] == "PASS":
            pass_count += 1
            print(f"PASS ({result['duration']:.1f}s)")
        else:
            fail_count += 1
            print(f"{result['status']} ({result['duration']:.1f}s)"
                  f" - {result['error']}")
            if args.stop_on_fail:
                break

    print()
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"Total: {args.runs}  Passed: {pass_count}  Failed: {fail_count}")

    if fail_count > 0:
        analyze_failures(results)
        sys.exit(1)
    else:
        print("\nAll runs passed!")
        sys.exit(0)


if __name__ == "__main__":
    main()
