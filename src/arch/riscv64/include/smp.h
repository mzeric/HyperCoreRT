#pragma once
#include "config.h"

#define SMP_MAX_CPUS CONFIG_SMP_CPU_NUM

/* Get current CPU ID — read from tp-based scratch structure */
int cpu_id(void);
int smp_cpu_count(void);
int smp_hart_id(int cpu);
void riscv_set_boot_hart_id(int hart_id);

/* Boot secondary harts via SBI HSM */
void smp_boot_secondaries(void);

/* Secondary hart C entry point */
#ifdef __cplusplus
extern "C" {
#endif
void secondary_start(int hart_id);
#ifdef __cplusplus
}
#endif
