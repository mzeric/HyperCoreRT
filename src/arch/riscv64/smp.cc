#include "smp.h"
#include "safe_printf.h"
#include "inline_asm.h"
#include "riscv64_system.h"
#include "sbi_helper.h"
#include "riscv_sbi.h"
#include "percpu.h"
#include "exception.h"
#include "ipi.h"
#include "kmalloc.h"
#include "mm.h"
#include "timer.h"
#include <string.h>

/* Per-CPU scratch structures — each hart gets its own */
static u64 g_scratch[SMP_MAX_CPUS][RISCV_SCRATCH_SIZE / sizeof(u64)];

/* Number of online CPUs */
static int g_cpu_count;
static int g_boot_hart_id = -1;
static int g_cpu_to_hart[SMP_MAX_CPUS];

int cpu_id(void) {
    /* tp points to the per-CPU scratch; slot 0 holds the CPU ID */
    u64 *scratch = (u64 *)csrr(CSR_SSCRATCH);
    /* For the boot hart during early init, sscratch may not be set yet */
    if (!scratch)
        return 0;
    return (int)scratch[RISCV_SCRATCH_SMP_ID_OFFSET / sizeof(u64)];
}

int smp_cpu_count(void) {
    return g_cpu_count;
}

int smp_hart_id(int cpu) {
    if (cpu < 0 || cpu >= g_cpu_count)
        return -1;
    return g_cpu_to_hart[cpu];
}

void riscv_set_boot_hart_id(int hart_id) {
    g_boot_hart_id = hart_id;
}

#define TRAP_STACK_PER_CPU 0x4000UL

extern char _trap_stack_top_[];

static void setup_scratch(int cpu) {
    u64 *s = g_scratch[cpu];
    memset(s, 0, RISCV_SCRATCH_SIZE);
    s[RISCV_SCRATCH_SMP_ID_OFFSET / sizeof(u64)] = (u64)cpu;
    s[RISCV_SCRATCH_EXCE_STACK_OFFSET / sizeof(u64)] =
        (u64)(_trap_stack_top_ - cpu * TRAP_STACK_PER_CPU);
}

/* Assembly entry for secondary harts — defined in head.S */
extern "C" void _start_secondary(void);

void smp_boot_secondaries(void) {
    g_cpu_count = 1;
    g_cpu_to_hart[0] = g_boot_hart_id;

    setup_scratch(0);
    /* Point primary's sscratch to its scratch area */
    csrw(CSR_SSCRATCH, (u64)&g_scratch[0]);

    for (int hart = 0; hart < SMP_MAX_CPUS; hart++) {
        if (hart == g_boot_hart_id)
            continue;
        if (g_cpu_count >= SMP_MAX_CPUS)
            break;

        int cpu = g_cpu_count;
        setup_scratch(cpu);

        struct sbiret ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_START,
                                       hart, (u64)_start_secondary,
                                       (u64)&g_scratch[cpu], 0, 0, 0);
        if (ret.error) {
            safe_printf("smp: failed to start hart %d (error %ld)\n", hart, ret.error);
            continue;
        }
        g_cpu_to_hart[cpu] = hart;
        g_cpu_count++;
        safe_printf("smp: started hart %d as cpu %d\n", hart, cpu);
    }

    safe_printf("smp: %d harts online\n", g_cpu_count);
}

/*
 * Secondary hart C entry point.
 * Called from _start_secondary in head.S with logical CPU ID in a0,
 * scratch pointer in a1.
 */
extern "C" void secondary_start(int cpu) {
    /* Set up sscratch for this CPU */
    csrw(CSR_SSCRATCH, (u64)g_scratch[cpu]);

    setup_exception((void *)__riscv_vector);
    riscv_secondary_mmu_init();
    csrw(CSR_SCOUNTEREN, 0x7);
    csrw(CSR_HCOUNTEREN, 0x7);
    ipi_pcpu_init();
    hyp_timer_rearm();

    safe_printf("smp: cpu %d (hart %d) online\n", cpu, smp_hart_id(cpu));

    csrs(sstatus, SSTATUS_SIE);
    csrs(sie, (1UL << IRQ_S_SOFT) | (1UL << IRQ_S_TIMER));

    /* Timer and reschedule IPIs enter sched_yield() from the trap handler. */
    while (1) {
        __asm__ volatile("wfi" ::: "memory");
    }
}
