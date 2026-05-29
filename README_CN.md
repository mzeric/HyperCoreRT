# HyperCoreRT

轻量级裸机虚拟机管理器（Hypervisor），面向 AArch64，专为实时和嵌入式场景设计。

## 概述

HyperCoreRT 运行在 ARMv8-A 处理器的 EL2 级别，支持虚拟化 Linux 和 RTOS 客户机。采用 Type-1 架构——虚拟机管理器直接接管硬件，无宿主操作系统。

**当前状态：** 可在 QEMU virt (aarch64) 上启动 Linux 客户机，支持在单个物理 CPU 上时间片调度最多 2 个客户 vCPU。SMP 调度、Stage-2 页表、虚拟 GICv3、虚拟定时器、虚拟 UART 均已可用。

## 特性

- **AArch64 EL2 Hypervisor** — 在 ARMv8-A 虚拟化扩展上裸机运行
- **Stage-2 地址转换** — LPAE 页表，通过 Access Flag Fault 实现按需映射
- **虚拟 GICv3** — 通过 ICH 列表寄存器注入 SGI/PPI/SPI 中断；支持 per-vCPU VGIC 状态保存/恢复
- **虚拟定时器** — EL1 物理/虚拟定时器的捕获与模拟
- **虚拟 UART (PL011)** — 宿主 UART 中断桥接到客户机 vIRQ，运行时通过 DTB 配置
- **vCPU 调度** — 协作式轮转调度，完整上下文切换（GPR、FP/SIMD、EL1 系统寄存器、定时器、独占监视器）
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

## 构建

HyperCoreRT 支持两套构建系统：**Makefile** 用于快速构建，**Bazel** 用于多架构开发。

### Makefile（快速开始）

#### 依赖

- `aarch64-none-elf-gcc`（ARM 裸机工具链，含 newlib）
- `dtc`（设备树编译器）
- `make`

#### 编译

```bash
make CROSS_COMPILE=aarch64-none-elf-
```

输出位于 `output/`：

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

- **把编译器做到了bazel的包管理器中了，不需要手动下载编译器，bazel编译的时候会自动下载，做到了新环境一键编译
- **基于 Platform 的工具链解析** — `--platforms=//:linux_aarch64` 自动选择正确的交叉编译器、编译选项和源文件，无需手动指定 `CROSS_COMPILE=` 或编写 `ifeq` 条件分支。
- **正确的增量编译** — 基于内容哈希而非时间戳做缓存。修改头文件时精确触发受影响的重新编译，不漏编也不多编。
- **沙箱隔离** — 编译动作只能访问显式声明的输入文件。缺少的 `-I` 路径在编译期即可发现，而非运行时因错误引用系统头文件导致崩溃。
- **外部依赖管理** — libfdt 和工具链通过 Bzlmod 自动拉取和版本管理，无需手动下载或 git submodule。

#### 用bazel编译

```bash
# AArch64
bazel build //:hyper 

```

#### 编译产物


`output/` 目录内容：

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


## 测试环境

- **平台：** QEMU virt 机器 (`qemu-system-aarch64 -M virt`)
- **CPU：** Cortex-A57，2 核 (`-smp 2`)
- **客户机：** Linux (aarch64) + initramfs
- **工具链：** GNU AArch64 裸机工具链 (aarch64-none-elf-gcc 10.x)

