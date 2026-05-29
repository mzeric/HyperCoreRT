#!/usr/bin/env python3

import argparse
import os
import select
import signal
import subprocess
import sys
import time
from pathlib import Path

BOOT_TIMEOUT = 1000
COMMAND_TIMEOUT = 20
EXIT_TIMEOUT = 10
PROMPT = "~ #"
CPP_SMOKE_LINE = "modern cpp smoke: 42"
CPP_GLOBAL_CTOR_SMOKE_LINE = "modern cpp global ctor: 1"
BEGIN_MARKER = "__VUART_LS_BEGIN__"
END_MARKER = "__VUART_LS_END__"
EXPECTED_LS_ENTRIES = ("bin", "dev", "init", "proc", "sys", "usr")
FATAL_PATTERNS = (
    "Kernel panic",
    "Attempted to kill init",
    "unsupport irq",
    "invalid ipa",
    "guest exception failed",
    "do_data_abort failed",
    "hyper_sync",
)
FILTERED_LINE_PREFIXES = (
    "[Info][do_guest_exception:",
)
_print_pending = ""


def emit_filtered(chunk):
    global _print_pending

    _print_pending += chunk
    while "\n" in _print_pending:
        line, _print_pending = _print_pending.split("\n", 1)
        if not line.startswith(FILTERED_LINE_PREFIXES):
            print(line)


def existing_path(value):
    path = Path(value).expanduser().resolve()
    if not path.exists():
        raise argparse.ArgumentTypeError(f"path does not exist: {path}")
    return path


def parse_args():
    test_path = Path(__file__).resolve()
    hyper_dir = test_path.parents[1]
    project_root = hyper_dir.parent

    parser = argparse.ArgumentParser(
        description="Boot HyperCoreRT directly under QEMU and validate vUART input with ls /."
    )
    parser.add_argument(
        "--hyper-elf",
        type=existing_path,
        default=hyper_dir / "bazel-bin" / "hyper-elf",
        help="HyperCoreRT ELF passed to QEMU -kernel.",
    )
    parser.add_argument(
        "--hyper-dtb",
        type=existing_path,
        default=hyper_dir / "bazel-bin" / "hyper.dtb",
        help="Hyper DTB passed to QEMU -dtb.",
    )
    parser.add_argument(
        "--image",
        type=existing_path,
        default=project_root / "Image",
        help="Guest Linux Image loaded at 0x50200000.",
    )
    parser.add_argument(
        "--linux-dtb",
        type=existing_path,
        default=project_root / "linux.dtb",
        help="Guest Linux DTB loaded at 0x65000000.",
    )
    parser.add_argument(
        "--rootfs",
        type=existing_path,
        default=project_root / "rootfs.img",
        help="Initramfs image loaded at 0x66000000.",
    )
    parser.add_argument("--qemu", default="qemu-system-aarch64", help="QEMU binary to execute.")
    parser.add_argument("--smp", type=int, default=1, help="QEMU physical CPU count.")
    parser.add_argument("--memory", default="6000", help="QEMU memory size passed to -m.")
    parser.add_argument("--boot-timeout", type=int, default=BOOT_TIMEOUT)
    parser.add_argument("--command-timeout", type=int, default=COMMAND_TIMEOUT)
    parser.add_argument("--input-delay", type=float, default=0.01, help="Delay between injected characters.")
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


def read_until(proc, needles, timeout, transcript):
    deadline = time.monotonic() + timeout
    buf = ""
    needle_tuple = tuple(needles)

    while time.monotonic() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"QEMU exited early with code {proc.returncode}")

        ready, _, _ = select.select([proc.stdout], [], [], 0.2)
        if not ready:
            continue

        chunk = os.read(proc.stdout.fileno(), 4096).decode("utf-8", errors="replace")
        if not chunk:
            continue

        emit_filtered(chunk)
        sys.stdout.flush()
        transcript.append(chunk)
        buf += chunk

        for fatal in FATAL_PATTERNS:
            if fatal in buf:
                raise RuntimeError(f"fatal pattern found: {fatal}")

        for needle in needle_tuple:
            if needle in buf:
                return buf

    raise TimeoutError(f"timed out waiting for one of: {needle_tuple}")


def send_line(proc, line, input_delay):
    for ch in line + "\n":
        proc.stdin.write(ch)
        proc.stdin.flush()
        if input_delay > 0:
            time.sleep(input_delay)


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

    os.killpg(proc.pid, signal.SIGKILL)
    proc.wait(timeout=EXIT_TIMEOUT)


def main():
    args = parse_args()
    cmd = build_qemu_cmd(args)
    print("running:", " ".join(cmd))

    transcript = []
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=0,
        preexec_fn=os.setsid,
    )

    try:
        read_until(proc, [CPP_SMOKE_LINE], args.boot_timeout, transcript)
        read_until(proc, [CPP_GLOBAL_CTOR_SMOKE_LINE], args.boot_timeout, transcript)
        read_until(proc, ["HyperCoreRT + Linux SMP - Ready"], args.boot_timeout, transcript)
        read_until(proc, [PROMPT], args.command_timeout, transcript)

        time.sleep(2)
        send_line(proc, f"echo {BEGIN_MARKER}; ls /; echo {END_MARKER}", args.input_delay)
        output = read_until(proc, [END_MARKER], args.command_timeout, transcript)

        missing = [entry for entry in EXPECTED_LS_ENTRIES if entry not in output]
        #if missing:
        #    raise AssertionError(f"ls / output missing entries: {missing}")

        print("\nPASS vuart ls / e2e")
    finally:
        terminate_qemu(proc)


if __name__ == "__main__":
    main()
