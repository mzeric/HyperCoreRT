#!/usr/bin/env python3
"""Build a riscv64 BusyBox initramfs for scripts/test_riscv.py."""

import argparse
import os
import shlex
import shutil
import stat
import subprocess
import tarfile
from pathlib import Path


READY_MARKER = "HyperCoreRT + Linux SMP - Ready"


def run(cmd, **kwargs):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, **kwargs)


def run_with_defaults(cmd):
    command = "yes '' | " + shlex.join(str(c) for c in cmd)
    print("+", command)
    return subprocess.run(command, shell=True, executable="/bin/bash").returncode


def set_config(config, key, value):
    lines = config.read_text().splitlines()
    set_line = f"{key}={value}"
    unset_line = f"# {key} is not set"
    replaced = False

    for i, line in enumerate(lines):
        if line.startswith(f"{key}=") or line == unset_line:
            lines[i] = set_line
            replaced = True
            break

    if not replaced:
        lines.append(set_line)

    config.write_text("\n".join(lines) + "\n")


def write_file(path, content, mode=0o644):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    path.chmod(mode)


def safe_extract_tar(tar_path, dest):
    dest = dest.resolve()
    if dest.exists():
        if not dest.name.endswith("_riscv_src"):
            raise SystemExit(f"refusing unsafe source dir cleanup: {dest}")
        shutil.rmtree(dest)
    dest.mkdir(parents=True)

    with tarfile.open(tar_path) as tar:
        for member in tar.getmembers():
            target = (dest / member.name).resolve()
            if dest not in target.parents and target != dest:
                raise SystemExit(f"unsafe path in tar: {member.name}")
        tar.extractall(dest)

    roots = [p for p in dest.iterdir() if p.is_dir()]
    if len(roots) != 1:
        raise SystemExit(f"unexpected BusyBox tar layout in {tar_path}")
    return roots[0]


def build_init_binary(rootfs, build_dir, cross_compile):
    source = build_dir / "riscv_init.c"
    source.write_text(f'''typedef long ssize_t;

#define AT_FDCWD       -100
#define O_RDWR         02
#define O_WRONLY       01
#define O_NONBLOCK     04000
#define F_SETFL        4
#define S_IFCHR        0020000

#define SYS_dup3       24
#define SYS_fcntl      25
#define SYS_mknodat    33
#define SYS_mount      40
#define SYS_openat     56
#define SYS_close      57
#define SYS_write      64
#define SYS_exit       93
#define SYS_execve     221

void *memcpy(void *dst, const void *src, unsigned long n) {{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}}

static long syscall6(long n, long a0, long a1, long a2, long a3, long a4, long a5) {{
    register long x10 __asm__("a0") = a0;
    register long x11 __asm__("a1") = a1;
    register long x12 __asm__("a2") = a2;
    register long x13 __asm__("a3") = a3;
    register long x14 __asm__("a4") = a4;
    register long x15 __asm__("a5") = a5;
    register long x17 __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(x10) : "r"(x11), "r"(x12), "r"(x13), "r"(x14), "r"(x15), "r"(x17) : "memory");
    return x10;
}}

static long sys_openat(long dirfd, const char *path, long flags, long mode) {{
    return syscall6(SYS_openat, dirfd, (long)path, flags, mode, 0, 0);
}}

static long sys_write(long fd, const char *buf, long len) {{
    return syscall6(SYS_write, fd, (long)buf, len, 0, 0, 0);
}}

static void write_file_msg(const char *path, const char *buf, long len) {{
    long fd = sys_openat(AT_FDCWD, path, O_WRONLY | O_NONBLOCK, 0);
    if (fd < 0)
        return;
    sys_write(fd, buf, len);
    syscall6(SYS_close, fd, 0, 0, 0, 0, 0);
}}

static void kmsg(const char *buf, long len) {{
    write_file_msg("/dev/kmsg", buf, len);
}}

static void sys_mount(const char *src, const char *target, const char *type) {{
    syscall6(SYS_mount, (long)src, (long)target, (long)type, 0, 0, 0);
}}

static void sys_mknodat(const char *path, long mode, long dev) {{
    syscall6(SYS_mknodat, AT_FDCWD, (long)path, mode, dev, 0, 0);
}}

static void attach_console(void) {{
    sys_mknodat("/dev/console", S_IFCHR | 0600, 0x501);
    sys_mknodat("/dev/ttyS0", S_IFCHR | 0600, 0x440);
    long fd = sys_openat(AT_FDCWD, "/dev/console", O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
        fd = sys_openat(AT_FDCWD, "/dev/ttyS0", O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
        return;
    syscall6(SYS_fcntl, fd, F_SETFL, 0, 0, 0, 0);
    syscall6(SYS_dup3, fd, 0, 0, 0, 0, 0);
    syscall6(SYS_dup3, fd, 1, 0, 0, 0, 0);
    syscall6(SYS_dup3, fd, 2, 0, 0, 0, 0);
    if (fd > 2)
        syscall6(SYS_close, fd, 0, 0, 0, 0, 0);
}}

void _start(void) {{
    attach_console();
    kmsg("RISCV init entered\\n", 19);
    sys_write(1, "RISCV init entered\\n", 19);
    sys_mount("proc", "/proc", "proc");
    sys_mount("sysfs", "/sys", "sysfs");
    kmsg("{READY_MARKER}\\n", {len(READY_MARKER) + 1});
    sys_write(1, "\\n========================================\\n", 42);
    sys_write(1, "  {READY_MARKER}\\n", {len(READY_MARKER) + 3});
    sys_write(1, "========================================\\n\\n", 42);

    char *argv[] = {{ (char *)"/bin/sh", (char *)"-i", (char *)0 }};
    char *envp[] = {{
        (char *)"HOME=/root",
        (char *)"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
        (char *)"TERM=vt100",
        (char *)0,
    }};
    syscall6(SYS_execve, (long)argv[0], (long)argv, (long)envp, 0, 0, 0);
    sys_write(2, "exec /bin/sh failed\\n", 20);
    syscall6(SYS_exit, 1, 0, 0, 0, 0, 0);
    for (;;) {{ }}
}}
''')
    run([f"{cross_compile}gcc", "-nostdlib", "-static", "-Os", "-fno-builtin",
         "-fno-stack-protector", "-ffreestanding", "-fno-pic", "-fno-pie", "-no-pie",
         "-msmall-data-limit=0", "-Wl,-e,_start", "-o", str(rootfs / "init"), str(source)])
    (rootfs / "init").chmod(0o755)


def prepare_rootfs(rootfs):
    for rel in ("dev", "etc/init.d", "proc", "root", "sys", "tmp"):
        (rootfs / rel).mkdir(parents=True, exist_ok=True)

    write_file(rootfs / "init", f"""#!/bin/sh

exec >/dev/console 2>&1
printf 'RISCV init entered\n'

mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null

printf '\n========================================\n'
printf '  {READY_MARKER}\n'
printf '========================================\n\n'

if [ -x /usr/bin/setsid ] && [ -x /bin/cttyhack ]; then
    exec /usr/bin/setsid /bin/cttyhack /bin/sh -i </dev/console >/dev/console 2>&1
fi

exec /bin/sh -i </dev/console >/dev/console 2>&1
""", 0o755)

    write_file(rootfs / "etc" / "fstab", """proc /proc proc defaults 0 0
sysfs /sys sysfs defaults 0 0
devtmpfs /dev devtmpfs defaults 0 0
""")
    write_file(rootfs / "etc" / "passwd", "root::0:0:root:/root:/bin/sh\n")
    write_file(rootfs / "etc" / "inittab", "::sysinit:/etc/init.d/rcS\n::respawn:/bin/sh\n")
    write_file(rootfs / "etc" / "init.d" / "rcS", "#!/bin/sh\n", 0o755)


def spec_line_for_path(rootfs, path):
    rel = "/" + str(path.relative_to(rootfs))
    st = path.lstat()
    mode = stat.S_IMODE(st.st_mode)

    if path.is_symlink():
        return f"slink {rel} {os.readlink(path)} 0777 0 0"
    if path.is_dir():
        return f"dir {rel} 0{mode:o} 0 0"
    if path.is_file():
        return f"file {rel} {path} 0{mode:o} 0 0"
    return None


def write_cpio_spec(rootfs, spec):
    entries = []
    for path in sorted(rootfs.rglob("*")):
        line = spec_line_for_path(rootfs, path)
        if line:
            entries.append(line)

    entries.extend([
        "nod /dev/console 0600 0 0 c 5 1",
        "nod /dev/null 0666 0 0 c 1 3",
        "nod /dev/zero 0666 0 0 c 1 5",
        "nod /dev/kmsg 0600 0 0 c 1 11",
        "nod /dev/tty 0666 0 0 c 5 0",
        "nod /dev/ttyS0 0600 0 0 c 4 64",
    ])
    spec.write_text("\n".join(entries) + "\n")


def main():
    script = Path(__file__).resolve()
    ci_dir = script.parents[1]
    project_root = ci_dir.parent
    busybox_dir = project_root / "busybox"

    parser = argparse.ArgumentParser(description="Build RISC-V BusyBox rootfs image.")
    parser.add_argument("--busybox-src", type=Path, default=None,
                        help="Existing BusyBox source tree. Defaults to extracting busybox-1.36.1.tar.bz2.")
    parser.add_argument("--busybox-tar", type=Path,
                        default=busybox_dir / "busybox-1.36.1.tar.bz2")
    parser.add_argument("--work-src", type=Path,
                        default=busybox_dir / "busybox-1.36.1_build_riscv_src")
    parser.add_argument("--build-dir", type=Path,
                        default=busybox_dir / "busybox-1.36.1_build_riscv")
    parser.add_argument("--rootfs-dir", type=Path,
                        default=busybox_dir / "rootfs-riscv")
    parser.add_argument("--output", type=Path,
                        default=project_root / "rootfs-riscv.img")
    parser.add_argument("--gen-init-cpio", type=Path,
                        default=project_root / "linux-5.4.291_build" / "usr" / "gen_init_cpio")
    parser.add_argument("--cross-compile", default="riscv64-linux-gnu-")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()

    if args.busybox_src:
        busybox_src = args.busybox_src.resolve()
    elif args.busybox_tar.exists():
        busybox_src = safe_extract_tar(args.busybox_tar.resolve(), args.work_src.resolve())
    else:
        busybox_src = (busybox_dir / "busybox-1.36.1").resolve()

    build_dir = args.build_dir.resolve()
    rootfs_dir = args.rootfs_dir.resolve()
    output = args.output.resolve()
    gen_init_cpio = args.gen_init_cpio.resolve()
    spec = busybox_dir / "rootfs-riscv.cpio.list"

    if not busybox_src.exists():
        raise SystemExit(f"missing BusyBox source: {busybox_src}")
    if not gen_init_cpio.exists():
        raise SystemExit(f"missing gen_init_cpio: {gen_init_cpio}")
    if rootfs_dir.name != "rootfs-riscv" or rootfs_dir.parent != busybox_dir.resolve():
        raise SystemExit(f"refusing unsafe rootfs dir: {rootfs_dir}")

    build_dir.mkdir(parents=True, exist_ok=True)
    config = build_dir / ".config"
    if not config.exists():
        ret = run_with_defaults(["make", "-C", str(busybox_src), f"O={build_dir}",
                                 "ARCH=riscv", f"CROSS_COMPILE={args.cross_compile}", "defconfig"])
        if ret != 0 and not config.exists():
            raise SystemExit("BusyBox defconfig failed")

    set_config(config, "CONFIG_STATIC", "y")
    ret = run_with_defaults(["make", "-C", str(busybox_src), f"O={build_dir}",
                             "ARCH=riscv", f"CROSS_COMPILE={args.cross_compile}", "oldconfig"])
    if ret != 0 and not config.exists():
        raise SystemExit("BusyBox oldconfig failed")

    run(["make", "-C", str(busybox_src), f"O={build_dir}",
         "ARCH=riscv", f"CROSS_COMPILE={args.cross_compile}", f"-j{args.jobs}"])

    if rootfs_dir.exists():
        shutil.rmtree(rootfs_dir)
    rootfs_dir.mkdir(parents=True)

    run(["make", "-C", str(busybox_src), f"O={build_dir}",
         "ARCH=riscv", f"CROSS_COMPILE={args.cross_compile}",
         f"CONFIG_PREFIX={rootfs_dir}", "install"])

    prepare_rootfs(rootfs_dir)
    build_init_binary(rootfs_dir, build_dir, args.cross_compile)
    write_cpio_spec(rootfs_dir, spec)

    output.parent.mkdir(parents=True, exist_ok=True)
    print("+", gen_init_cpio, spec, ">", output)
    with output.open("wb") as out:
        subprocess.run([str(gen_init_cpio), str(spec)], check=True, stdout=out)

    size = output.stat().st_size
    print(f"rootfs: {output} ({size} bytes)")
    print(f"rootfs dir: {rootfs_dir}")
    print(f"cpio spec: {spec}")


if __name__ == "__main__":
    main()
