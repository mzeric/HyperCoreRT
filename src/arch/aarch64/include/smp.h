#pragma once

#include "config.h"
#include <stdint.h>

/* Maximum host physical CPUs */
#define SMP_MAX_CPUS  CONFIG_SMP_CPU_NUM

/* CPU online states */
enum cpu_state {
    CPU_OFFLINE = 0,
    CPU_ONLINE_PARKED,
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
void secondary_start(void);
void smp_boot_secondaries(void *fdt);
