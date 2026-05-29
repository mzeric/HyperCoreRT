# SMP pCPU Phase 1 Implementation Plan

## 目标

让 secondary pCPU 在 QEMU virt 上启动起来，进入 EL2，使用独立 EL2 栈，完成最小 CPU-local 初始化后 parked。

Phase 1 不运行 guest vCPU，不进入调度器，不修改共享 runqueue。目标只是证明 Host 物理 SMP bring-up 路径可靠。

## 实现状态：已完成

3 个 commit 已落地：

1. `0904847` — head.S: secondary_entry + per-CPU stack + MPIDR split
2. `e24b4d0` — smp.c: PSCI CPU_ON + DTB /cpus parse + secondary_start()
3. 文档更新

## 关键设计决策

### 为什么用 PSCI 而不是 spin-table

QEMU virt 默认只启动 CPU0，secondary 处于 powered-off 状态。
只有 PSCI CPU_ON (`smc #0`) 能唤醒 secondary，spin-table 需要额外 EL3 firmware。
`hyper.dts` `/cpus` 已改为 `enable-method = "psci"`。

### Secondary CPU 的入口

PSCI CPU_ON 的 entry point 直接指向 `secondary_entry`（head.S 中的独立 label），
不走 `_start`。secondary 在 head.S 中完成：MPIDR 识别、per-CPU 栈选择、VBAR_EL2 设置，
然后跳到 C 函数 `secondary_start()`。

### Secondary MMU 状态

PSCI 启动的 secondary MMU 状态不确定。Phase 1 方案：
- secondary 不 enable MMU
- GIC init 使用物理地址（不经过 ioremap）
- `gicv3_cpu_init()` 只用 sysreg，不需要 MMIO 映射
|------|------|
| `src/arch/aarch64/head.S` | 增加 CPU ID 判断、per-CPU stack 选择、secondary entry |
| `src/arch/aarch64/smp.c` (新建) | host SMP bring-up、CPU map、secondary init/park |
| `src/arch/aarch64/include/smp.h` (新建) | SMP API 声明 |
| `src/arch/aarch64/init.c` | primary boot 完成基础初始化后调用 `smp_boot_secondary()` |
| `src/arch/aarch64/linker.lds` | 暴露 `_hvc_stack_start/_hvc_stack_end` 已有，无需大改；必要时补 align |
| `include/config.h` | 如需要，确认 `CONFIG_SMP_CPU_NUM >= 2` |

## Step 1: Early CPU ID 映射

新增：

```c
#define INVALID_CPU_ID (-1)

int smp_mpidr_to_cpu(uint64_t mpidr);
uint64_t smp_cpu_to_mpidr(int cpu);
int smp_current_cpu_id(void);
```

Phase 1 就从 DTB `/cpus/cpu@*/reg` 填充 MPIDR table，不做固定数组硬编码。

`reg` 是 MPIDR affinity value，不是 linear CPU ID。例如：

```text
0x000 = Aff1=0, Aff0=0
0x100 = Aff1=1, Aff0=0
0x001 = Aff1=0, Aff0=1
```

当前 `hyper.dts` 写的是：

```dts
cpu@0 { reg = <0x0 0x000>; };
cpu@1 { reg = <0x0 0x100>; };
```

因此当前配置下映射是：

```text
MPIDR 0x000 -> cpu_id 0
MPIDR 0x100 -> cpu_id 1
```

但这只是当前 DTB 配置结果，不是 QEMU virt 的通用常量。实现必须以 DTB 为准。

`mpidr` 必须 mask 掉非 affinity bits，与 `smp_id()` 保持一致。

## Step 2: Per-CPU EL2 栈选择

当前 `head.S`：

```asm
ldr x0, =_hvc_stack_end
mov sp, x0
```

改为：

1. 读 `MPIDR_EL1`
2. 转换为 early linear CPU ID
3. 根据 CPU ID 选择栈：

```text
stack_top(cpu) = _hvc_stack_start + CONFIG_INT_STACK_SIZE * (cpu + 1)
```

注意：
- 如果 CPU ID 无法识别，进入 hang loop，不要共用 CPU0 stack。
- stack 选择必须发生在调用 C 函数前。

## Step 3: Primary / Secondary 分流

`head.S` 中选完栈后：

```text
if cpu_id == 0:
    bl _reset
else:
    bl secondary_start
```

新增 C 函数：

```c
void secondary_start(void);
```

secondary 入口不走完整 `_reset`，避免重复：
- zero bss
- init_mm
- init_gicv3 distributor
- init_kmalloc
- create guest task

## Step 4: Secondary 最小初始化

`secondary_start()` 执行：

1. 确认当前 EL 是 EL2；如果从 EL3 进来，后续再补 EL3→EL2，Phase 1 先以 QEMU 当前路径为准。
2. 写 `VBAR_EL2 = &__hyp_vectors`
3. 执行 CPU-local init：
   - `cpu_init()` 或拆出 `cpu_init_secondary()`
   - GIC CPU interface init
   - timer CPU-local init
4. 打印：

```text
[Info][secondary_start:<line>]pcpu1 online, mpidr=0x100
```

5. 设置状态：

```c
secondary_cpu_state[cpu] = CPU_ONLINE_PARKED;
```

6. 进入 parked loop：

```c
while (1) {
    wfi();
}
```

## Step 5: Primary release secondary

在 primary 完成：

- UART 可用
- DTB 已解析
- MMU 已可用
- GIC distributor 初始化完成

之后调用：

```c
smp_boot_secondary();
```

`smp_boot_secondary()`：

1. 解析 `hyper.dts` `/cpus` 子节点
2. 跳过 boot CPU
3. 对每个 `enable-method = "spin-table"` 的 CPU：
   - 读取 `cpu-release-addr`
   - 将 `secondary_entry` 物理地址写入 release addr
   - `dsb sy`
   - `sev`
4. 等待 `secondary_cpu_state[cpu] == CPU_ONLINE_PARKED`
5. 超时则打印 warn，不影响 CPU0 继续跑

## Step 6: DTB 解析接口

新增 helper：

```c
struct host_cpu_desc {
    uint64_t mpidr;
    uint64_t release_addr;
    int enabled;
};

int parse_host_cpus(void *fdt, struct host_cpu_desc *cpus, int max_cpus);
```

解析规则：

- path: `/cpus`
- 遍历 `cpu@*`
- `reg` → MPIDR affinity
- `enable-method` 只支持 `spin-table`
- `cpu-release-addr` 必须存在

Phase 1 先支持当前 `hyper.dts` 中声明的 CPU。超过 `CONFIG_SMP_CPU_NUM` 的 CPU 忽略并 warn；缺少 `reg` 或 `cpu-release-addr` 的节点也 warn。

## Step 7: 验证标准

### Build

```bash
bazel build //:hyper --platforms=//:linux_aarch64
```

### QEMU

```bash
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

### 期望日志

```text
pcpu0 boot
release pcpu1 mpidr=0x100 addr=...
pcpu1 online, mpidr=0x100
pcpu1 parked
HyperCoreRT boot finished
```

### 通过条件

- CPU0 仍能正常启动 Linux guest
- CPU1 有独立日志，进入 parked loop
- 没有 stack corruption
- 没有重复初始化 bss/mm/gic distributor/kmalloc
- 多次启动稳定

## 风险点

1. **QEMU secondary boot 方式差异**
   - 如果 QEMU 已经让所有 CPU 从 `_start` 进入，则 `head.S` 分流即可。
   - 如果 secondary 真在 spin-table firmware holding pen，需要 primary 写 `cpu-release-addr` + `sev`。
   - Phase 1 需要同时兼容这两种情况。

2. **stack 选择必须在 C 调用前完成**
   - 不能先调用 C 再算 CPU ID，否则多个 CPU 可能共用同一个 SP。

3. **MMU 状态**
   - secondary 进来时可能与 primary MMU 状态不同。
   - Phase 1 先在 QEMU 上验证实际行为，再决定是否需要 secondary 单独 enable MMU。

4. **GICR 尚未 per-CPU 正式映射**
   - Phase 1 可以只初始化 GIC CPU interface，GICR frame 精确映射放到 Phase 4。
   - 但如果 timer PPI 需要 secondary 接收中断，则必须提前初始化对应 GICR。

5. **MPIDR 编码不是固定拓扑**
   - 当前 `hyper.dts` 是 `0x0` / `0x100`，但其他 QEMU topology 或真实硬件可能是 `0x0` / `0x1` 或更多层级 affinity。
   - 所有映射必须来自 DTB `/cpus/reg`，不能把 `0x100` 写死成 CPU1。

## 不在 Phase 1 范围内

- secondary 运行 vCPU
- scheduler SMP 化
- vCPU migration
- host IPI
- TLB shootdown
- per-CPU runqueue
- allocator/VGIC/log 全面加锁
