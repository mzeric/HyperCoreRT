# HyperCore

A lightweight baremetal hypervisor for AArch64, designed for real-time and embedded workloads.

## Overview

HyperCore runs at EL2 on ARMv8-A processors and supports virtualizing Linux and RTOS guests. It uses a type-1 architecture — the hypervisor owns the hardware directly with no host OS underneath.

**Current status:** Boots Linux on QEMU virt (aarch64) with up to 2 guest vCPUs time-sliced on a single physical CPU. SMP scheduling, stage-2 page tables, virtual GICv3, virtual timer, and virtual UART are functional.

## Features

- **AArch64 EL2 hypervisor** — runs baremetal on ARMv8-A with virtualization extensions
- **Stage-2 address translation** — LPAE page tables with lazy mapping via access flag faults
- **Virtual GICv3** — SGI, PPI, SPI injection via ICH list registers; per-vCPU VGIC state save/restore
- **Virtual timer** — EL1 physical/virtual timer trapping and emulation
- **Virtual UART (PL011)** — host UART IRQ bridging to guest vIRQ via runtime config from DTB
- **vCPU scheduling** — cooperative round-robin with full context switch (GPR, FP/SIMD, EL1 sysregs, timers, exclusive monitor)
- **DTB-driven configuration** — guest memory map, vCPU count/MPIDR, vGIC, UART bridge all parsed from the hypervisor device tree
- **C++ support** — minimal baremetal C++ runtime with global constructors, no exceptions/RTTI

## Architecture

```
┌──────────────────────────────────────────────┐
│                Guest Linux                    │  EL0/EL1
│  (or RTOS)                                   │
├──────────────────────────────────────────────┤
│              HyperCore                        │  EL2
│  ┌─────────┐ ┌────────┐ ┌─────────────────┐ │
│  │ vCPU     │ │ vGICv3 │ │ Stage-2 MMU     │ │
│  │ Sched    │ │ Emul   │ │ (LPAE)          │ │
│  └─────────┘ └────────┘ └─────────────────┘ │
│  ┌─────────┐ ┌────────┐ ┌─────────────────┐ │
│  │ vTimer   │ │ vUART  │ │ DTB Config      │ │
│  │ Emul     │ │ Bridge │ │ Parser          │ │
│  └─────────┘ └────────┘ └─────────────────┘ │
├──────────────────────────────────────────────┤
│              Hardware / QEMU virt             │  EL3
└──────────────────────────────────────────────┘
```

## Build

HyperCore supports two independent build systems: Makefile for local cross-compilation and quick validation, and Bazel for multi-architecture toolchain management and CI-style builds.

### Build System Boundary

Makefile and Bazel are separate entry points. They do not call each other and do not share toolchain resolution logic:

| Entry | Toolchain source | Architecture selection | Output directory | Use case |
|-------|------------------|------------------------|------------------|----------|
| Makefile | Locally installed cross toolchain selected by `CROSS_COMPILE` | `TARGET=aarch64` or `TARGET=riscv64` | `output/<target>/` | Local cross-compilation, quick validation |
| Bazel | Bzlmod/toolchain resolution downloads and registers toolchains | `--platforms=//:linux_aarch64` or `--platforms=//:linux_riscv64` | `bazel-bin/` and `output/` | Fresh-environment builds, CI, multi-arch toolchain management |

Makefile uses the local toolchain prefix from `CROSS_COMPILE=/path/to/prefix-`. Bazel does not use Makefile's `CROSS_COMPILE`; its compiler is selected by the platform/toolchain configuration in `MODULE.bazel` and `BUILD`.

### Makefile (Quick Start)

#### Prerequisites

- AArch64: `aarch64-none-elf-gcc/g++/objcopy` or a compatible bare-metal/newlib cross toolchain
- RISC-V: `riscv-none-elf-gcc/g++/objcopy` or a compatible bare-metal/newlib cross toolchain
- `dtc` (device tree compiler)
- `make`

#### Compile

```bash
# AArch64
make TARGET=aarch64

# RISC-V
make TARGET=riscv64

# Equivalent shortcut targets
make aarch64
make riscv64
```

`TARGET` selects the architecture-specific sources, include directory, linker script, and default cross-compiler prefix. Output is written to `output/<target>/`.

Makefile parameters:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TARGET` | `aarch64` | Target architecture; supports `aarch64` and `riscv64` |
| `CROSS_COMPILE` | Depends on `TARGET` | Cross-compiler prefix; AArch64 defaults to `aarch64-none-elf-`, RISC-V defaults to `riscv-none-elf-` |
| `DTC` | `dtc` | Device tree compiler path |
| `OUT` | `output/<target>` | Output directory |
| `BUILD_DIR` | `build/<target>` | Intermediate build directory |

Override `CROSS_COMPILE` if your local toolchain uses a different prefix:

```bash
make TARGET=riscv64 CROSS_COMPILE=riscv64-unknown-elf-
make TARGET=aarch64 CROSS_COMPILE=aarch64-linux-gnu-
```

The default `*-none-elf` bare-metal/newlib toolchains are used because the hypervisor runs in a bare-metal environment while still relying on libc/newlib symbols such as `printf`, `memset`, `memmove`, `memcmp`, `strlen`, and `strnlen`. Linux glibc toolchains such as `riscv64-linux-gnu-` are usually not suitable for the current `rv64imac/lp64` bare-metal configuration.

Makefile uses `-std=gnu++17`. The codebase follows C++17, but the current low-level code also uses GNU extensions such as `typeof` and statement expressions.

| File | Description |
|------|-------------|
| `hyper-elf` | Hypervisor ELF binary |
| `core.bin` | Raw binary (for QEMU `-kernel`) |
| `hyper.dtb` | Compiled device tree blob |

#### Clean

```bash
make clean
```

### Bazel (Multi-Architecture)

#### Prerequisites

- [Bazel 9.x](https://github.com/bazelbuild/bazel/releases)

#### Why Bazel

This project targets multiple CPU architectures (aarch64, riscv64) with different toolchains, compiler flags, source files, and per-arch drivers. Bazel provides several advantages over Makefile for this scenario:

- **Toolchain package management** - compilers are managed as Bazel external dependencies and can be fetched during the build.
- **Platform-based toolchain resolution** - `--platforms=//:linux_aarch64` or `--platforms=//:linux_riscv64` selects the correct compiler, flags, and sources.
- **Correct incremental builds** - content-hash based caching, not timestamps.
- **Hermetic sandboxing** - build actions can only access explicitly declared inputs.
- **External dependency management** - libfdt and toolchains are versioned through Bzlmod/local overrides.

#### Build with Bazel

```bash
# AArch64
bazel build //:hyper --platforms=//:linux_aarch64

# RISC-V
bazel build //:hyper --platforms=//:linux_riscv64
```

Note: AArch64 and RISC-V Bazel outputs use the same names, including `bazel-bin/core.bin`, `bazel-bin/hyper-elf`, and `bazel-bin/hyper.dtb`. Rebuild the target architecture before running it.

#### Build Artifacts

Bazel outputs are mainly under `bazel-bin/`:

| File | Description |
|------|-------------|
| `hyper-elf` | Hypervisor ELF binary |
| `core.bin` | Raw binary (for QEMU `-kernel`) |
| `hyper.dtb` | Compiled device tree blob |

# How to Configure Hyper

## Device Tree Configuration (hyper.dts)

`hyper.dts` is the hypervisor's configuration entry point, defining physical hardware topology, guest resource allocation, and device bridging. Below are the key nodes to configure.

### Guest Configuration

```dts
guest0 {
    compatible = "hypervisor,guest";
    guest-entry = <0x0 0x50200000>;   /* Guest kernel entry address */
    guest-dtb   = <0x0 0x65000000>;   /* Guest DTB / initramfs load address */
    vcpu-count  = <2>;                 /* Number of vCPUs */
    vcpu-mpidrs = <0x0 0x0 0x0 0x1>;  /* MPIDR affinity per vCPU */

    uart@9000000 {                     /* Virtualized UART for guest */
        compatible = "arm,pl011";
        reg = <0x0 0x09000000 0x0 0x1000>;
    };

    memory@guest0 {                    /* Guest physical memory range */
        reg = <0x0 0x40000000 0x0 0x20000000>;  /* 512MB */
    };

    vgic@8000000 {                     /* Virtual GICv3 config */
        compatible = "hypervisor,vgic-v3";
        reg = <0x0 0x08000000 0 0x10000>,       /* VGICD */
              <0x0 0x080a0000 0 0xf60000>;      /* VGICR */
    };
};
```

- `guest-entry` must match the QEMU `-device loader` address
- `vcpu-mpidrs` uses two cells per vCPU (affinity value), count = `vcpu-count × 2`
- Guest `memory` defines **IPA (Intermediate Physical Address)** range, mapped to real physical memory via Stage-2 page tables

### Physical Memory Layout

```dts
memory@0 {
    reg = <0x0 0x80080000 0x0 0x80000000>;  /* 2GB, starts at 0x8008_0000 */
};

reserved-memory {
    reserved: device_memory@831000000 {
        no-map;
        reg = <0xb 0x00000000 0x1 0x00000000>;  /* Reserved for device MMIO */
    };
};
```

- `memory@0` is the physical memory managed by the hypervisor
- `reserved-memory` regions are excluded from guest allocation

## Running on QEMU

### Prepare Guest Images

Prepare a compiled Linux kernel Image and rootfs.

### Launch Command

```bash
# Using the script (recommended)
./scripts/run_qemu_aarch64.sh

# With rootfs
IMAGE=Image ROOTFS=rootfs ./scripts/run_qemu_aarch64.sh

# Manual launch
qemu-system-aarch64 \
    -M virt,virtualization=true,gic-version=3,secure=on \
    -cpu cortex-a57 \
    -smp 2 \
    -m 2G \
    -nographic \
    -kernel output/core.bin \
    -dtb output/hyper.dtb \
    -device loader,file=Image,addr=0x50200000 \
    -device loader,file=rootfs.cpio.gz,addr=0x65000000
```

Key parameters:

| Parameter | Description |
|-----------|-------------|
| `-M virt,virtualization=true` | Enable virtualization extensions (EL2) |
| `-gic-version=3` | Use GICv3 interrupt controller |
| `-secure=on` | Enable EL3 (secure state) |
| `-kernel output/core.bin` | Hypervisor raw binary, QEMU loads at `0x40080000` |
| `-dtb output/hyper.dtb` | Hypervisor device tree describing guest config |
| `-device loader,file=Image,addr=0x50200000` | Load guest kernel at `guest-entry` address |
| `-device loader,file=rootfs.cpio.gz,addr=0x65000000` | Load rootfs at `guest-dtb` address |

## Tested Environment

- **Platform:** QEMU virt machine (`qemu-system-aarch64 -M virt`)
- **CPU:** Cortex-A57, 2 cores (`-smp 2`)
- **Guest:** Linux (aarch64) with initramfs
- **Toolchain:** GNU Toolchain for AArch64 bare-metal (aarch64-none-elf-gcc 10.x)
