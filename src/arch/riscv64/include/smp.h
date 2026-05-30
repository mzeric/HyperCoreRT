#pragma once

/* RISC-V SMP stubs — Phase 1 single-hart only */

#define SMP_MAX_CPUS 1

static inline int cpu_id(void) { return 0; }

static inline int smp_cpu_count(void) { return 1; }
