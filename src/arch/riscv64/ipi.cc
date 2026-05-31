/*
 * RISC-V IPI implementation using SBI send_ipi.
 */
#include "ipi.h"
#include "sbi_helper.h"
#include "smp.h"
#include "mm.h"
#include "mmu.h"
#include "inline_asm.h"
#include "safe_printf.h"

/* Per-CPU IPI message pending bits */
volatile u32 g_ipi_pending[SMP_MAX_CPUS];

/* Accessor for exception handler */
extern u32 ipi_get_pending(int cpu) {
    return g_ipi_pending[cpu];
}

extern void ipi_clear_pending(int cpu) {
    g_ipi_pending[cpu] = 0;
}

/* TLB shootdown callback */
static ipi_func_t g_tlb_shootdown_fn;

void ipi_send_cpu(int target_cpu, uint8_t ipi_vec) {
    if (target_cpu < 0 || target_cpu >= smp_cpu_count())
        return;

    int hart = smp_hart_id(target_cpu);
    if (hart < 0)
        return;

    g_ipi_pending[target_cpu] |= (1u << ipi_vec);

    unsigned long hart_mask = 1UL << hart;
    sbi_send_ipi(&hart_mask);
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
    }
}

void ipi_pcpu_init(void) {
    g_ipi_pending[cpu_id()] = 0;
}

void ipi_tlb_shootdown(void) {
    ipi_broadcast_others(IPI_TLB_SHOOTDOWN);
    hfence();
}

void ipi_call_func(int target_cpu, ipi_func_t fn, void *arg) {
    g_tlb_shootdown_fn = fn;
    ipi_send_cpu(target_cpu, IPI_CALL_FUNC);
}
