#!/usr/bin/env python3
"""
RISC-V hypervisor QEMU test runner.

Usage:
    python3 scripts/test_riscv.py
    python3 scripts/test_riscv.py --mode boot --runs 10
    python3 scripts/test_riscv.py --mode linux --guest-cpus 2 -v
    python3 scripts/test_riscv.py --mode shell -v
    python3 scripts/test_riscv.py --mode stress --runs 10 --stress-rounds 20
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
EXIT_TIMEOUT = 5
GUEST_DTB_SIZE = 0x10000

CPP_SMOKE = "modern cpp smoke: 42"
CPP_CTOR = "modern cpp global ctor: 1"
CPP_RAI = "modern cpp raii lock: 99"
RISCV_BOOT = "HyperCoreRT RISC-V booting"
GUEST_HELLO = "hello,guest"
SBI_DIAG = "[sbi_diag] base=1 time=1 ipi=1 rfence=1 hsm=0"
SBI_TIME_OK = "[sbi_diag] time_set_timer=ok"
SBI_RFENCE_BAD_FID_OK = "[sbi_diag] rfence_bad_fid=ok"
LINUX_BOOT = "Linux version"
LINUX_VFS_PANIC = "Kernel panic - not syncing: VFS"
READY_MARKER = "HyperCoreRT + Linux SMP - Ready"
PASS_MARKER = "[stress] ALL PASS"
FAIL_MARKER = "[stress] SOME FAILURES"
SHELL_BEGIN = "__RISCV_SHELL_BEGIN__"
SHELL_END = "__RISCV_SHELL_END__"
PROMPT_MARKERS = ("~ #", "/ #", "# ")
EXPECTED_LS_ENTRIES = ("bin", "dev", "init", "proc", "sys", "usr")
LINUX_MODES = {"linux", "shell", "stress"}

FATAL_PATTERNS = (
    "Kernel panic",
    "Oops -",
    "trap error",
    "Attempted to kill init",
)
NO_ROOTFS_FATAL_PATTERNS = (
    "Oops -",
    "trap error",
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def existing_path(value):
    path = Path(value).expanduser().resolve()
    if not path.exists():
        raise argparse.ArgumentTypeError(f"path does not exist: {path}")
    return path


def parse_int(value):
    return int(value, 0)


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
    parser.add_argument("--mode", choices=["boot", "linux", "shell", "stress"],
                        default="boot",
                        help="Test mode: boot, linux, shell, or stress.")
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
                        help="Path to bare guest binary (ELF or raw). "
                        "Default: auto-detect from bazel-bin.")
    parser.add_argument("--linux-image", type=existing_path,
                        default=project_root / "linux-5.4.291_build" / "arch" / "riscv" / "boot" / "Image",
                        help="Linux Image path for Linux-backed modes.")
    parser.add_argument("--rootfs", type=existing_path,
                        default=project_root / "rootfs-riscv.img",
                        help="RISC-V initramfs image for Linux rootfs modes.")
    parser.add_argument("--guest-load-addr", type=parse_int, default=0x90200000,
                        help="Guest Linux load/entry GPA for Linux-backed modes.")
    parser.add_argument("--guest-dtb-addr", type=parse_int, default=0x90f00000,
                        help="Generated guest DTB GPA for Linux-backed modes.")
    parser.add_argument("--guest-ram-base", type=parse_int, default=0x90000000,
                        help="Guest RAM base GPA for Linux-backed modes.")
    parser.add_argument("--guest-ram-size", type=parse_int, default=0x08000000,
                        help="Guest RAM size for Linux-backed modes.")
    parser.add_argument("--guest-cpus", type=int, default=1,
                        help="Guest vCPU count advertised in generated DTB for Linux-backed modes.")
    parser.add_argument("--initrd-load-addr", type=parse_int, default=0x96000000,
                        help="Guest physical load address for rootfs initramfs.")
    parser.add_argument("--no-rootfs", action="store_true",
                        help="Boot Linux without initrd and expect the VFS panic marker.")
    parser.add_argument("--stress-rounds", type=int, default=20,
                        help="Number of shell stress loop rounds.")
    parser.add_argument("--qemu", default=qemu_default)
    parser.add_argument("--boot-timeout", type=int, default=BOOT_TIMEOUT)
    parser.add_argument("--command-timeout", type=int, default=COMMAND_TIMEOUT)
    parser.add_argument("--input-delay", type=float, default=0.01)
    parser.add_argument("-v", "--verbose", action="store_true")
    return parser.parse_args()


def resolve_guest_bin(args, ci_dir):
    """Resolve guest binary path — Linux Image for Linux modes, bare guest otherwise."""
    if args.mode in LINUX_MODES:
        return Path(args.linux_image).expanduser().resolve()
    if args.guest_bin:
        path = Path(args.guest_bin).expanduser().resolve()
        if not path.exists():
            raise FileNotFoundError(f"guest binary not found: {path}")
        return path
    candidates = [
        ci_dir / "bazel-bin" / "test" / "arch" / "riscv64" / "guest-elf",
        ci_dir / "bazel-bin" / "test" / "arch" / "riscv64" / "guest.bin",
    ]
    for c in candidates:
        if c.exists():
            return c
    raise FileNotFoundError(
        f"guest binary not found, tried: {[str(c) for c in candidates]}")


def ranges_overlap(a_start, a_end, b_start, b_end):
    return a_start < b_end and b_start < a_end


def validate_initrd_range(args, initrd_end):
    ram_start = args.guest_ram_base
    ram_end = args.guest_ram_base + args.guest_ram_size
    dtb_start = args.guest_dtb_addr
    dtb_end = args.guest_dtb_addr + GUEST_DTB_SIZE

    if args.initrd_load_addr < ram_start or initrd_end > ram_end:
        raise ValueError(
            f"initrd range 0x{args.initrd_load_addr:x}-0x{initrd_end:x} "
            f"outside guest RAM 0x{ram_start:x}-0x{ram_end:x}")
    if ranges_overlap(args.initrd_load_addr, initrd_end, dtb_start, dtb_end):
        raise ValueError(
            f"initrd range 0x{args.initrd_load_addr:x}-0x{initrd_end:x} "
            f"overlaps guest DTB 0x{dtb_start:x}-0x{dtb_end:x}")


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

    if args.mode in LINUX_MODES:
        hyper_args = [
            f"guest_entry=0x{args.guest_load_addr:x}",
            f"guest_dtb=0x{args.guest_dtb_addr:x}",
            f"guest_ram_base=0x{args.guest_ram_base:x}",
            f"guest_ram_size=0x{args.guest_ram_size:x}",
            f"guest_vcpus={args.guest_cpus}",
        ]
        cmd += ["-device", f"loader,file={guest_bin},force-raw=on,addr=0x{args.guest_load_addr:x}"]

        if not args.no_rootfs:
            rootfs = Path(args.rootfs).expanduser().resolve()
            initrd_end = args.initrd_load_addr + rootfs.stat().st_size
            validate_initrd_range(args, initrd_end)
            hyper_args += [
                f"guest_initrd_start=0x{args.initrd_load_addr:x}",
                f"guest_initrd_end=0x{initrd_end:x}",
            ]
            cmd += ["-device", f"loader,file={rootfs},force-raw=on,addr=0x{args.initrd_load_addr:x}"]

        cmd += ["-append", " ".join(hyper_args)]
        return cmd

    # Load bare guest binary at 0x90080000 via QEMU generic loader.
    if guest_bin.suffix == ".bin":
        cmd += ["-device", f"loader,file={guest_bin},addr=0x90080000"]
    else:
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


def drain_output(proc, output_buf, verbose=False, duration=1.0):
    """Drain QEMU output briefly after a fatal marker so logs include trap details."""
    deadline = time.monotonic() + duration
    while time.monotonic() < deadline:
        ready, _, _ = select.select([proc.stdout], [], [], 0.1)
        if not ready:
            continue
        chunk = os.read(proc.stdout.fileno(), 8192).decode("utf-8", errors="replace")
        if not chunk:
            continue
        output_buf.append(chunk)
        if verbose:
            sys.stdout.write(chunk)
            sys.stdout.flush()


def read_until(proc, needles, timeout, output_buf, verbose=False, fatal_patterns=FATAL_PATTERNS):
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

        for fatal in fatal_patterns:
            if fatal in text:
                drain_output(proc, output_buf, verbose)
                raise RuntimeError(f"fatal pattern: {fatal}")

        for needle in needles:
            if needle in text:
                return text

    raise TimeoutError(f"timed out waiting for: {needles}")


def send_line(proc, line, delay):
    for ch in line + "\n":
        proc.stdin.write(ch.encode())
        proc.stdin.flush()
        if delay > 0:
            time.sleep(delay)


# ---------------------------------------------------------------------------
# Test modes
# ---------------------------------------------------------------------------

def get_text(output_parts):
    return "".join(output_parts)


def wait_for_prompt(args, proc, output_parts):
    text = get_text(output_parts)
    if any(prompt in text for prompt in PROMPT_MARKERS):
        return text
    return read_until(proc, PROMPT_MARKERS, args.command_timeout, output_parts, args.verbose)


def test_boot(args, proc, output_parts):
    """Wait for hypervisor boot markers and bare guest output."""
    read_until(proc, [CPP_SMOKE], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [CPP_CTOR], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [CPP_RAI], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, ["sstatus:"], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [GUEST_HELLO], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [SBI_DIAG], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [SBI_TIME_OK], args.boot_timeout, output_parts, args.verbose)
    read_until(proc, [SBI_RFENCE_BAD_FID_OK], args.boot_timeout, output_parts, args.verbose)

    time.sleep(1)

    text = get_text(output_parts)
    if RISCV_BOOT not in text:
        raise AssertionError("RISC-V boot marker not found")


def test_linux(args, proc, output_parts):
    """Boot Linux with rootfs by default, or to VFS panic with --no-rootfs."""
    fatal = NO_ROOTFS_FATAL_PATTERNS if args.no_rootfs else FATAL_PATTERNS
    read_until(proc, [CPP_SMOKE], args.boot_timeout, output_parts, args.verbose, fatal)
    read_until(proc, [CPP_CTOR], args.boot_timeout, output_parts, args.verbose, fatal)
    read_until(proc, [CPP_RAI], args.boot_timeout, output_parts, args.verbose, fatal)
    read_until(proc, ["creating guest task"], args.boot_timeout, output_parts, args.verbose, fatal)
    read_until(proc, [LINUX_BOOT], args.boot_timeout, output_parts, args.verbose, fatal)

    if args.no_rootfs:
        read_until(proc, [LINUX_VFS_PANIC], args.boot_timeout, output_parts, args.verbose, fatal)
    else:
        read_until(proc, [READY_MARKER], args.boot_timeout, output_parts, args.verbose, fatal)


def test_shell(args, proc, output_parts):
    """Boot Linux rootfs and validate shell I/O over UART."""
    test_linux(args, proc, output_parts)
    wait_for_prompt(args, proc, output_parts)

    begin_cmd = "printf '%s\\n' __RISCV_''SHELL_BEGIN__"
    end_cmd = "printf '%s\\n' __RISCV_''SHELL_END__"
    send_line(proc, f"{begin_cmd}; ls /; uname -a; {end_cmd}", args.input_delay)
    output = read_until(proc, [SHELL_END], args.command_timeout, output_parts, args.verbose)

    missing = [entry for entry in EXPECTED_LS_ENTRIES if entry not in output]
    if missing:
        raise AssertionError(f"ls / missing entries: {missing}")


def test_stress(args, proc, output_parts):
    """Boot Linux rootfs and run a shell-driven stress loop."""
    test_linux(args, proc, output_parts)
    wait_for_prompt(args, proc, output_parts)

    rounds = max(1, args.stress_rounds)
    pass_cmd = "printf '%s\\n' '[stress] ALL 'PASS"
    fail_cmd = "printf '%s\\n' '[stress] SOME 'FAILURES"
    cmd = (
        "printf '%s\\n' __RISCV_''STRESS_BEGIN__; "
        "i=0; ok=1; "
        f"while [ $i -lt {rounds} ]; do "
        "echo \"[stress] round $i\"; "
        "uname -a >/dev/null || ok=0; "
        "cat /proc/cpuinfo >/dev/null || ok=0; "
        "ls / >/dev/null || ok=0; "
        "dmesg >/dev/null 2>&1 || true; "
        "i=$((i+1)); "
        "done; "
        f"if [ $ok -eq 1 ]; then {pass_cmd}; else {fail_cmd}; fi"
    )
    send_line(proc, cmd, args.input_delay)

    output = read_until(proc, [PASS_MARKER, FAIL_MARKER],
                        args.command_timeout, output_parts, args.verbose)
    if FAIL_MARKER in output:
        raise AssertionError("In-guest stress test reported failures")
    if PASS_MARKER not in output:
        raise AssertionError("Stress test result marker not found")


TEST_FUNCS = {
    "boot": test_boot,
    "linux": test_linux,
    "shell": test_shell,
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
    if args.mode in LINUX_MODES and not args.no_rootfs:
        rootfs = Path(args.rootfs).expanduser().resolve()
        print(f"Rootfs: {rootfs}")
        print(f"Initrd: 0x{args.initrd_load_addr:x}-0x{args.initrd_load_addr + rootfs.stat().st_size:x}")

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
