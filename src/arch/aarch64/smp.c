#include "smp.h"
#include "safe_printf.h"
#include "inline_asm.h"

/*
 * Secondary pCPU entry point.
 * Called from head.S secondary_entry after per-CPU stack and VBAR_EL2 are set.
 * Phase 1: just print and park.
 */
void secondary_start(void)
{
    int cpu = 0; /* TODO: proper cpu_id mapping */
    uint64_t mpidr = smp_id();

    safe_printf("pcpu? online, mpidr=0x%lx", mpidr);

    /* Park */
    while (1) {
        __asm__ volatile("wfi" ::: "memory");
    }
}
