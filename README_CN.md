# HyperCoreRT

轻量级裸机虚拟机管理器（Hypervisor），面向 AArch64 和 RISC-V，专为实时和嵌入式场景设计。

## 概述

HyperCoreRT 在 ARMv8-A 上运行于 EL2，在 RISC-V 上基于 H-extension 运行，支持虚拟化 Linux 和 RTOS 客户机。采用 Type-1 架构——虚拟机管理器直接接管硬件，无宿主操作系统。

**当前状态：**

- **AArch64：** 可在 QEMU virt (aarch64) 上启动 Linux 客户机，支持 2 pCPU / 2 vCPU SMP 场景。SMP 调度、Stage-2 页表、虚拟 GICv3、虚拟定时器、虚拟 UART 均已可用。
- **RISC-V：** 可在 QEMU virt (riscv64) 上启动 Linux + rootfs，支持 2 pCPU / 2 vCPU 静态绑定。Stage-2 地址转换、虚拟 SBI（BASE/TIME/IPI/RFENCE）、虚拟定时器、PLIC/vIRQ、虚拟 UART、Guest Fault Manager 均已可用。

## 特性

- **AArch64 EL2 Hypervisor** — 在 ARMv8-A 虚拟化扩展上裸机运行
- **RISC-V H-extension Hypervisor** — 在 RISC-V 虚拟化扩展上裸机运行，支持 VS/VU 客户机执行环境
- **Stage-2 地址转换** — LPAE 页表，通过 Access Flag Fault 实现按需映射
- **虚拟 GICv3** — 通过 ICH 列表寄存器注入 SGI/PPI/SPI 中断；支持 per-vCPU VGIC 状态保存/恢复
- **RISC-V PLIC/vIRQ** — 支持 PLIC 外部中断接收、虚拟中断投递和每 vCPU pending IRQ 管理
- **虚拟定时器** — EL1 物理/虚拟定时器的捕获与模拟
- **RISC-V 虚拟 SBI** — 支持 BASE、TIME、IPI、RFENCE 等现代 SBI 扩展的客户机虚拟化
- **虚拟 UART (PL011)** — 宿主 UART 中断桥接到客户机 vIRQ，运行时通过 DTB 配置
- **vCPU 调度** — 协作式轮转调度，完整上下文切换（GPR、FP/SIMD、EL1 系统寄存器、定时器、独占监视器）
- **Guest Fault Manager** — 将客户机异常、Stage-2 fault、MMIO 模拟和非法指令注入从通用 trap handler 中拆分，避免客户机异常直接拖垮 Hypervisor
- **DTB 驱动配置** — 客户机内存映射、vCPU 数量/MPIDR、vGIC、UART 桥接均从虚拟机管理器设备树解析
- **C++ 支持** — 最小化裸机 C++ 运行时，支持全局构造函数，无异常/RTTI

## 架构

```
┌──────────────────────────────────────────────┐
│                Guest Linux                    │  EL0/EL1
│  (或 RTOS)                                   │
├──────────────────────────────────────────────┤
│              HyperCoreRT                      │  EL2
│  ┌─────────┐ ┌────────┐ ┌─────────────────┐ │
│  │ vCPU     │ │ vGICv3 │ │ Stage-2 MMU     │ │
│  │ 调度器   │ │ 模拟   │ │ (LPAE)          │ │
│  └─────────┘ └────────┘ └─────────────────┘ │
│  ┌─────────┐ ┌────────┐ ┌─────────────────┐ │
│  │ vTimer   │ │ vUART  │ │ DTB 配置        │ │
│  │ 模拟     │ │ 桥接   │ │ 解析器          │ │
│  └─────────┘ └────────┘ └─────────────────┘ │
├──────────────────────────────────────────────┤
│              硬件 / QEMU virt                 │  EL3
└──────────────────────────────────────────────┘
```

RISC-V 路径的核心结构如下：

```
┌──────────────────────────────────────────────┐
│                Guest Linux                    │  VS/VU
├──────────────────────────────────────────────┤
│              HyperCoreRT                      │  HS
│  ┌─────────────┐ ┌─────────────┐ ┌──────────┐ │
│  │ vCPU / Trap │ │ Virtual SBI │ │ Stage-2  │ │
│  │ Manager     │ │ TIME/IPI/RF │ │ HGATP    │ │
│  └─────────────┘ └─────────────┘ └──────────┘ │
│  ┌─────────────┐ ┌─────────────┐ ┌──────────┐ │
│  │ vTimer      │ │ vIRQ/PLIC   │ │ vUART    │ │
│  │ Manager     │ │ Manager     │ │ Bridge   │ │
│  └─────────────┘ └─────────────┘ └──────────┘ │
├──────────────────────────────────────────────┤
│         OpenSBI / QEMU virt / RISC-V H-ext     │
└──────────────────────────────────────────────┘
```

## 构建

HyperCoreRT 支持两套互相独立的构建系统：Makefile 用于无 Bazel 环境下的本地交叉编译，Bazel 用于多架构工具链管理和 CI 式构建。

### 构建系统边界

Makefile 和 Bazel 是两套独立入口，不互相调用，也不共享工具链解析逻辑：

| 构建入口 | 工具链来源 | 架构选择方式 | 输出目录 | 适用场景 |
|----------|------------|--------------|----------|----------|
| Makefile | 本机已安装的交叉工具链，通过 `CROSS_COMPILE` 指定 | `TARGET=aarch64` 或 `TARGET=riscv64` | `output/<target>/` | 无 Bazel 环境、快速本地交叉编译 |
| Bazel | Bzlmod/toolchain resolution 自动拉取并注册工具链 | `--platforms=//:linux_aarch64` 或 `--platforms=//:linux_riscv64` | `bazel-bin/` 和 `output/` | 新环境一键构建、CI、多架构工具链管理 |

Makefile 假设环境中没有 Bazel，因此不会使用 Bazel 下载的工具链，也不会解析 `MODULE.bazel`。如果本机 PATH 中没有默认工具链，需要通过 `CROSS_COMPILE=/path/to/prefix-` 显式指定。Bazel 则不使用 Makefile 的 `CROSS_COMPILE`，由 `MODULE.bazel` 和 `BUILD` 中的 platform/toolchain 配置决定编译器。

### Makefile（快速开始）

#### 依赖

- AArch64：`aarch64-none-elf-gcc/g++/objcopy` 或兼容交叉工具链
- RISC-V：`riscv-none-elf-gcc/g++/objcopy` 或兼容裸机/newlib 交叉工具链
- `dtc`（设备树编译器）
- `make`

#### 编译

```bash
# AArch64
make TARGET=aarch64

# RISC-V
make TARGET=riscv64

# 等价快捷目标
make aarch64
make riscv64
```

Makefile 不依赖 Bazel。`TARGET` 选择架构后，Makefile 会选择对应的源码、架构 include、linker script 和默认交叉编译器前缀。输出位于 `output/<target>/`：

Makefile 参数说明：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `TARGET` | `aarch64` | 选择目标架构，支持 `aarch64`、`riscv64` |
| `CROSS_COMPILE` | 随 `TARGET` 变化 | 交叉编译器前缀；AArch64 默认 `aarch64-none-elf-`，RISC-V 默认 `riscv-none-elf-` |
| `DTC` | `dtc` | 设备树编译器路径 |
| `OUT` | `output/<target>` | 输出目录 |
| `BUILD_DIR` | `build/<target>` | 中间文件目录 |

如果本机工具链前缀不同，可以显式覆盖：

```bash
make TARGET=riscv64 CROSS_COMPILE=riscv64-unknown-elf-
make TARGET=aarch64 CROSS_COMPILE=aarch64-linux-gnu-
```

注意：RISC-V 默认要求裸机/newlib 工具链。`riscv64-linux-gnu-` 这类 Linux glibc 工具链通常不适合当前 `rv64imac/lp64` 裸机配置。

Makefile 使用 `-std=gnu++17`。代码基线仍是 C++17，但当前代码使用了 `typeof`、statement expression 等 GNU 扩展，因此直编入口需要启用 GNU C++17 方言。

| 文件 | 说明 |
|------|------|
| `hyper-elf` | Hypervisor ELF 二进制 |
| `core.bin` | 裸机二进制（供 QEMU `-kernel` 使用） |
| `hyper.dtb` | 编译后的设备树 Blob |

#### 清理

```bash
make clean
```

### Bazel（多架构构建）

#### 依赖

- [Bazel 9.x](https://github.com/bazelbuild/bazel/releases)

#### 为什么用 Bazel

本项目面向多种 CPU 架构（aarch64、riscv64），不同架构有不同的工具链、编译选项、源文件和架构专属驱动。Bazel 在此场景下相比 Makefile 有以下优势：

- **工具链包管理** — 编译器作为 Bazel 外部依赖管理，不需要手动下载；Bazel 编译时会自动拉取，支持新环境一键编译。
- **基于 Platform 的工具链解析** — `--platforms=//:linux_aarch64` 自动选择正确的交叉编译器、编译选项和源文件，无需手动指定 `CROSS_COMPILE=` 或编写 `ifeq` 条件分支。
- **正确的增量编译** — 基于内容哈希而非时间戳做缓存。修改头文件时精确触发受影响的重新编译，不漏编也不多编。
- **沙箱隔离** — 编译动作只能访问显式声明的输入文件。缺少的 `-I` 路径在编译期即可发现，而非运行时因错误引用系统头文件导致崩溃。
- **外部依赖管理** — libfdt 和工具链通过 Bzlmod 自动拉取和版本管理，无需手动下载或 git submodule。

#### 用 Bazel 编译

```bash
# AArch64
bazel build //:hyper --platforms=//:linux_aarch64

# RISC-V
bazel build //:hyper --platforms=//:linux_riscv64

```

注意：AArch64 和 RISC-V 的 Bazel 产物名称相同，都会生成 `bazel-bin/core.bin`、`bazel-bin/hyper-elf`、`bazel-bin/hyper.dtb` 等文件。切换架构运行前需要先重新执行对应架构的 Bazel build，确保当前输出目录中的产物属于目标架构。

#### 编译产物

Bazel 主要产物位于 `bazel-bin/`：

| 文件 | 说明 |
|------|------|
| `hyper-elf` | Hypervisor ELF 二进制 |
| `core.bin` | 裸机二进制（供 QEMU `-kernel` 使用） |
| `hyper.dtb` | 编译后的设备树 Blob |


# 怎么配置hyper 

## 设备树配置 (hyper.dts)

`hyper.dts` 是 Hypervisor 的配置入口，定义了物理硬件拓扑、Guest 资源分配和外设桥接。以下是需要关注的核心节点。

### Guest 配置

```dts
guest0 {
    compatible = "hypervisor,guest";
    guest-entry = <0x0 0x50200000>;   /* Guest 内核入口地址 */
    guest-dtb   = <0x0 0x65000000>;   /* Guest DTB / initramfs 加载地址 */
    vcpu-count  = <2>;                 /* 分配的 vCPU 数量 */
    vcpu-mpidrs = <0x0 0x0 0x0 0x1>;  /* 每个 vCPU 的 MPIDR 值 */

    uart@9000000 {                     /* 为 Guest 虚拟化的 UART */
        compatible = "arm,pl011";
        reg = <0x0 0x09000000 0x0 0x1000>;
    };

    memory@guest0 {                    /* Guest 物理内存范围 */
        reg = <0x0 0x40000000 0x0 0x20000000>;  /* 512MB */
    };

    vgic@8000000 {                     /* 虚拟 GICv3 配置 */
        compatible = "hypervisor,vgic-v3";
        reg = <0x0 0x08000000 0 0x10000>,       /* VGICD */
              <0x0 0x080a0000 0 0xf60000>;      /* VGICR */
    };
};
```

- `guest-entry` 必须与 QEMU `-device loader` 的 `addr` 一致
- `vcpu-mpidrs` 每个 vCPU 占两个 cell（affinity 值），数量 = `vcpu-count × 2`
- Guest 的 `memory` 节点定义 **IPA（中间物理地址）** 范围，由 Stage-2 页表映射到实际物理内存

### 物理内存布局

```dts
memory@0 {
    reg = <0x0 0x80080000 0x0 0x80000000>;  /* 2GB，起始 0x8008_0000 */
};

reserved-memory {
    reserved: device_memory@831000000 {
        no-map;
        reg = <0xb 0x00000000 0x1 0x00000000>;  /* 为设备 MMIO 预留 */
    };
};
```

- `memory@0` 是 Hypervisor 自身可管理的物理内存范围
- `reserved-memory` 标记的区间不会被分配给 Guest

## 在Qemu中运行

### 准备 Guest 镜像

准备编译好的Linux kernel Image，和 rootfs
### 启动命令

```bash
# 使用脚本（推荐）
./scripts/run_qemu_aarch64.sh

# 带 rootfs
IMAGE=Image ROOTFS=rootfs ./scripts/run_qemu_aarch64.sh

# 手动运行
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

关键参数说明：

| 参数 | 说明 |
|------|------|
| `-M virt,virtualization=true` | 启用虚拟化扩展，Hypervisor 可运行在 EL2 |
| `-gic-version=3` | 使用 GICv3 中断控制器 |
| `-secure=on` | 启用 EL3（安全态） |
| `-kernel build/core.bin` | Hypervisor 裸机二进制，QEMU 加载到 `0x40080000` |
| `-dtb build/hyper.dtb` | Hypervisor 设备树，描述 Guest 配置 |
| `-device loader,file=Image,addr=0x50200000` | 将 Guest 内核加载到 `guest-entry` 指定地址 |
| `-device loader,file=rootfs.cpio.gz,addr=0x65000000` | 将 rootfs 加载到 `guest-dtb` 指定地址 |

### RISC-V QEMU 运行

RISC-V 路径使用 Bazel 构建产物，并通过 QEMU loader 加载客户机 Linux Image 和 initramfs。

```bash
# 1. 构建 RISC-V Hypervisor
bazel build //:hyper --platforms=//:linux_riscv64

# 2. 运行 RISC-V stress 测试（2 pCPU / 2 vCPU）
python3 scripts/test_riscv.py \
    --mode stress \
    --runs 1 \
    --stress-rounds 20 \
    --smp 2 \
    --guest-cpus 2
```

等价的核心 QEMU 参数如下：

```bash
qemu-system-riscv64 \
    -M virt \
    -nographic \
    -bios default \
    -smp 2 \
    -m 512 \
    -no-reboot \
    -kernel bazel-bin/hyper-elf \
    -device loader,file=../linux-5.4.291_build/arch/riscv/boot/Image,force-raw=on,addr=0x90200000 \
    -device loader,file=../rootfs-riscv.img,force-raw=on,addr=0x96000000 \
    -append "guest_entry=0x90200000 guest_dtb=0x90f00000 guest_ram_base=0x90000000 guest_ram_size=0x8000000 guest_vcpus=2 guest_initrd_start=0x96000000 guest_initrd_end=<initrd_end>"
```

RISC-V 关键参数说明：

| 参数 | 说明 |
|------|------|
| `-M virt` | 使用 QEMU RISC-V virt 机器，提供 PLIC、ACLINT、UART 等基础设备 |
| `-bios default` | 使用 OpenSBI 作为 M-mode firmware |
| `-kernel bazel-bin/hyper-elf` | 加载 RISC-V Hypervisor ELF |
| `guest_entry` | Guest Linux 入口地址，需与 `-device loader` 加载地址一致 |
| `guest_dtb` | Hypervisor 生成并放置 Guest DTB 的地址 |
| `guest_vcpus` | 暴露给 Guest Linux 的 vCPU 数量 |
| `guest_initrd_start/end` | Guest initramfs 的物理加载范围 |


## 测试环境

- **AArch64 平台：** QEMU virt 机器 (`qemu-system-aarch64 -M virt,virtualization=true,gic-version=3,secure=on`)
- **AArch64 CPU：** Cortex-A57，2 核 (`-smp 2`)
- **AArch64 客户机：** Linux (aarch64) + initramfs
- **RISC-V 平台：** QEMU virt 机器 (`qemu-system-riscv64 -M virt`)，OpenSBI v1.7
- **RISC-V CPU：** 2 pCPU / 2 vCPU，当前采用静态 vCPU:pCPU 绑定
- **RISC-V 客户机：** Linux 5.4.291 (riscv64) + initramfs
- **构建系统：** Bazel 9.x，多架构 toolchain resolution
- **回归测试：** `scripts/test_riscv.py --mode stress --smp 2 --guest-cpus 2` 和 `scripts/test_hyper.py --mode stress --smp 2`

