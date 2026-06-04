/*
 * RISC-V IPI implementation using SBI send_ipi.
 */
#include "ipi.h"
#include "riscv_ipi.h"
#include "sbi_helper.h"
#include "smp.h"
#include "mm.h"
#include "mmu.h"
#include "inline_asm.h"
#include "safe_printf.h"
#include "arch_barrier.h"
#include "riscv_features.h"
#include "timer.h"

#define RISCV_IPI_SYNC_TIMEOUT_USEC 100000UL
#define RISCV_IPI_SYNC_TIMEOUT_MIN_CYCLES 1000UL

/* Per-CPU IPI message pending bits */
volatile u32 g_ipi_pending[SMP_MAX_CPUS];
static volatile u32 g_ipi_complete[SMP_MAX_CPUS][IPI_MAX];

static int ipi_validate_target(int target_cpu, uint8_t ipi_vec, int *hart) {
    if (ipi_vec >= IPI_MAX)
        return -1;
    if (target_cpu < 0 || target_cpu >= smp_cpu_count())
        return -1;

    *hart = smp_hart_id(target_cpu);
    if (*hart < 0)
        return -1;
    return 0;
}

uint32_t riscv_ipi_take_pending(int cpu) {
    if (cpu < 0 || cpu >= SMP_MAX_CPUS)
        return 0;
    return __sync_lock_test_and_set(&g_ipi_pending[cpu], 0);
}

/* TLB shootdown callback */
static ipi_func_t g_tlb_shootdown_fn;

static void ipi_remote_fence_all(void) {
    fence_i();
    hfence();
}

static u64 riscv_ipi_sync_timeout_cycles(void) {
    u64 cycles = ((u64)riscv_timebase_frequency() * RISCV_IPI_SYNC_TIMEOUT_USEC) / 1000000UL;
    return cycles ? cycles : RISCV_IPI_SYNC_TIMEOUT_MIN_CYCLES;
}

void ipi_send_cpu(int target_cpu, uint8_t ipi_vec) {
    int hart;
    if (ipi_validate_target(target_cpu, ipi_vec, &hart) != 0)
        return;

    __sync_fetch_and_or(&g_ipi_pending[target_cpu], (1u << ipi_vec));
    arch_smp_wmb();

    unsigned long hart_mask = 1UL << hart;
    sbi_send_ipi(&hart_mask);
}

int riscv_ipi_send_cpu_sync(int target_cpu, uint8_t ipi_vec) {
    int hart;
    if (ipi_validate_target(target_cpu, ipi_vec, &hart) != 0)
        return -1;
    if (target_cpu == cpu_id()) {
        ipi_handle(ipi_vec);
        return 0;
    }

    u32 before = g_ipi_complete[target_cpu][ipi_vec];
    __sync_fetch_and_or(&g_ipi_pending[target_cpu], (1u << ipi_vec));
    arch_smp_wmb();

    unsigned long hart_mask = 1UL << hart;
    sbi_send_ipi(&hart_mask);

    u64 start = get_cycles();
    u64 timeout = riscv_ipi_sync_timeout_cycles();
    while (g_ipi_complete[target_cpu][ipi_vec] == before) {
        if (get_cycles() - start >= timeout) {
            safe_printf("riscv ipi sync timeout: target=%d vec=%u pending=0x%x complete=%u\n",
                        target_cpu, ipi_vec, g_ipi_pending[target_cpu],
                        g_ipi_complete[target_cpu][ipi_vec]);
            return -1;
        }
        arch_cpu_relax();
    }
    arch_smp_mb();
    return 0;
}

void ipi_send_reschedule(int target_cpu) {
    ipi_send_cpu(target_cpu, IPI_RESCHEDULE);
}

void ipi_broadcast_others(uint8_t ipi_vec) {
    int me = cpu_id();
    for (int i = 0; i < smp_cpu_count(); i++) {
        if (i != me)
            ipi_send_cpu(i, ipi_vec);
    }
}

void ipi_handle(uint8_t ipi_vec) {
    switch (ipi_vec) {
    case IPI_RESCHEDULE:
        /* Will trigger sched_yield on next timer tick */
        break;
    case IPI_TLB_SHOOTDOWN:
        /* Execute TLB flush */
        hfence();
        if (g_tlb_shootdown_fn)
            g_tlb_shootdown_fn(NULL);
        break;
    case IPI_CALL_FUNC:
        break;
    case IPI_REMOTE_FENCE_ALL:
        ipi_remote_fence_all();
        break;
    }

    if (ipi_vec < IPI_MAX)
        __sync_fetch_and_add(&g_ipi_complete[cpu_id()][ipi_vec], 1);
}

void ipi_pcpu_init(void) {
    g_ipi_pending[cpu_id()] = 0;
    for (int vec = 0; vec < IPI_MAX; vec++)
        g_ipi_complete[cpu_id()][vec] = 0;
}

void ipi_tlb_shootdown(void) {
    ipi_broadcast_others(IPI_TLB_SHOOTDOWN);
    hfence();
}

void ipi_call_func(int target_cpu, ipi_func_t fn, void *arg) {
    g_tlb_shootdown_fn = fn;
    ipi_send_cpu(target_cpu, IPI_CALL_FUNC);
}
