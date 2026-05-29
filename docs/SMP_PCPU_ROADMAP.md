# SMP pCPU Roadmap

当前状态：Guest 虚拟 SMP 已可用（2 vCPU 时间片调度），但 Host 物理 SMP 还未启用——Hypervisor 目前只实际使用 1 个 pCPU。

目标：支持多个物理 CPU 并行运行，每个 pCPU 可独立调度 vCPU。

## 已有的基础设施

| 组件 | 状态 | 位置 |
|------|------|------|
| Spinlock | 已实现，调度器尚未使用 | `src/arch/aarch64/spin_lock.c` |
| Per-CPU 声明框架 | 有脚手架，尚未接入真实 CPU offset | `src/arch/aarch64/include/percpu.h` |
| TPIDR_EL2 读取 | 有 helper，尚未写入 | `src/arch/aarch64/include/inline_asm.h` |
| TLBI broadcast | 已实现 inner-shareable 变体 | `src/arch/aarch64/include/inline_asm.h` |
| EL2 栈空间 | linker 已预留 `CONFIG_SMP_CPU_NUM` 份 | `src/arch/aarch64/linker.lds` |
| GIC SGI 常量 | 有定义，尚未实现 host IPI 发送 | `src/drivers/gic/gicv3.h` |
| Barriers | dmb/dsb/isb 齐全 | `src/arch/aarch64/include/arch_barrier.h` |

## Phase 0: SMP Guardrail

**目标：** 在系统真正 SMP-safe 前，secondary CPU 只能启动并 parked，不参与调度，不修改共享状态。

| 步骤 | 内容 |
|------|------|
| 0.1 | 明确 primary CPU 是唯一执行完整 boot path 的 CPU |
| 0.2 | secondary CPU 只执行最小 EL2 初始化，然后进入 parked idle |
| 0.3 | 所有 secondary 输出只允许 minimal log，避免调度器/allocator/VGIC 等共享状态被并发访问 |

## Phase 1: Secondary CPU 启动

**目标：** CPU1 跑起来，进入 EL2，有独立 EL2 栈，能打印日志，然后 parked。

| 步骤 | 内容 | 涉及文件 |
|------|------|----------|
| 1.1 | 从 DTB `/cpus/cpu@*/reg` 建立 early MPIDR → linear CPU ID 映射；当前 `hyper.dts` 是 `0x0` / `0x100`，但实现不能硬编码 | `src/arch/aarch64/smp.c` (新建) |
| 1.2 | `head.S` 按 CPU ID 选择对应 EL2 栈，避免所有 pCPU 共用 `_hvc_stack_end` | `src/arch/aarch64/head.S` |
| 1.3 | 增加 secondary 入口：设置 SP、切到/确认 EL2、调用 secondary init | `src/arch/aarch64/head.S` |
| 1.4 | primary 解析 DTB `/cpus`，获取 `cpu-release-addr`，写 secondary entry，执行 `sev` | `src/arch/aarch64/init.c` |
| 1.5 | secondary 执行最小初始化：`VBAR_EL2`、CPU sysregs、GIC CPU interface、timer，然后 parked | `src/arch/aarch64/smp.c` |

**注意：** MPIDR affinity 不是 linear CPU ID。比如 `0x100` 表示 `Aff1=1, Aff0=0`，不能直接当数组下标；必须显式映射成 `cpu_id=1`。

## Phase 2: Per-CPU 基础设施

**目标：** 每个 pCPU 有自己的 per-CPU 区域和当前任务指针。

| 步骤 | 内容 |
|------|------|
| 2.1 | 用 TPIDR_EL2 保存当前 CPU 的 per-CPU base，或统一改造 `this_cpu()` 走 linear CPU ID |
| 2.2 | 修复 `__per_cpu_offset[]`：启动时按 CPU ID 分配偏移，而不是全 0 |
| 2.3 | `g_current_task` 从全局变量改为 per-CPU 变量 `this_cpu(current_task)` |
| 2.4 | 提供稳定的 `cpu_id()` API，禁止直接用 raw `smp_id()` 当数组下标 |

## Phase 3: 调度器 SMP 化

**目标：** 多个 pCPU 独立调度不同 vCPU。

第一版选择：**global ready queue + spinlock + static vCPU pinning**。先不做 work stealing，降低复杂度。

| 步骤 | 内容 |
|------|------|
| 3.1 | 调度器加锁：`sched_yield`、`g_ready_list`、`g_wait_list` 用 spinlock 保护 |
| 3.2 | 每个 pCPU 从共享 ready queue 取任务，任务结构增加 `pcpu_id` / affinity |
| 3.3 | 初期静态 pinning：vCPU0 → pCPU0，vCPU1 → pCPU1 |
| 3.4 | secondary 从 parked 状态切到 scheduler loop，开始运行 pinned vCPU |
| 3.5 | 后续再演进为 per-CPU run queue / work stealing |

## Phase 4: GIC 多 CPU + IPI

**目标：** 每个 pCPU 独立管理 Redistributor，支持 pCPU 间 IPI。

| 步骤 | 内容 |
|------|------|
| 4.1 | 遍历 GICR_TYPER，根据 MPIDR 找到每个 pCPU 对应的 Redistributor frame |
| 4.2 | secondary 启动时初始化自己的 GICR + GIC CPU interface |
| 4.3 | 实现 host 物理 SGI：写 `ICC_SGI1R_EL1` 给目标 pCPU 发 IPI |
| 4.4 | IPI 用途：reschedule IPI、TLB shootdown |

## Phase 5: 全局状态 SMP 安全

**目标：** 共享数据结构无 data race。

| 步骤 | 内容 |
|------|------|
| 5.1 | 内存分配器加锁：page allocator、kmalloc/TLSF |
| 5.2 | log ring buffer 加锁，或改成 per-CPU log buffer，避免多 pCPU 并发写坏 buffer |
| 5.3 | VGIC 共享状态加锁：`emul_gicv3.c` 全局结构、vCPU pending virq 队列 |
| 5.4 | Stage-2 页表操作加锁：`s2_map`/`s2_unmap` + TLB shootdown IPI |
| 5.5 | vUART 共享状态加锁：host UART RX 分发给目标 pCPU 上的 vCPU |
| 5.6 | Guest memory region list / emulated device list 加锁 |

## Phase 6: 验证

| 步骤 | 内容 |
|------|------|
| 6.1 | QEMU `-smp 2` 启动，确认 secondary CPU 进入 parked idle |
| 6.2 | 每个 pCPU 初始化独立 GICR + EL2 stack，log 中能区分 CPU ID |
| 6.3 | vCPU0/vCPU1 分别 pinned 到 pCPU0/pCPU1，Linux SMP 正常启动 |
| 6.4 | 现有 `test_boot_stress.py` 压测通过 |
| 6.5 | 新增 IPI 压测、并发 log 压测、TLB shootdown 压测 |

## 依赖关系

```
Phase 0 (guardrail)
        ↓
Phase 1 (secondary boot)
        ↓
Phase 2 (percpu)
        ↓
Phase 3 (scheduler)
        ↕
Phase 4 (GIC/IPI) + Phase 5 (SMP safety)
        ↓
Phase 6 (verification)
```
