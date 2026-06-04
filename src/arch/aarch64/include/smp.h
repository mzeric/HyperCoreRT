#pragma once

#include "config.h"
#include "inline_asm.h"
#include <stdint.h>

/* Maximum host physical CPUs */
#define SMP_MAX_CPUS  CONFIG_SMP_CPU_NUM

/* Fast linear CPU ID — read from TPIDR_EL2, set by head.S at boot. */
static inline int cpu_id(void)
{
    return (int)thread_id();
}

/* CPU online states */
enum cpu_state {
    CPU_OFFLINE = 0,
    CPU_ONLINE_PARKED,
    CPU_ONLINE_SCHED,
};

/* Per-host-CPU descriptor */
struct host_cpu_desc {
    uint64_t mpidr;        /* MPIDR affinity value (masked) */
    int      cpu_id;       /* linear CPU index */
    volatile enum cpu_state state;
};

/* API */
int  smp_mpidr_to_cpu(uint64_t mpidr);
int  smp_current_cpu_id(void);
uint64_t smp_cpu_to_mpidr(int cpu);
int  smp_cpu_count(void);
void secondary_start(void);
void smp_boot_secondaries(void *fdt);

