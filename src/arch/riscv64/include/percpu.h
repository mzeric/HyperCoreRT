#pragma once
#include "config.h"

/* RISC-V per-CPU stubs — Phase 1 single-hart */
extern char __per_cpu_start[];
extern unsigned long __per_cpu_offset[CONFIG_SMP_CPU_NUM];

#define DEFINE_PER_CPU(type, name) \
    __typeof__(type) per_cpu__##name __attribute__((section(".bss")))

#define this_cpu(var) ((void *)&per_cpu__##var)

static inline void init_percpu_area(void) {}
