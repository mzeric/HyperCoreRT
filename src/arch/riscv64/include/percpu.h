#pragma once
#include "config.h"
#include "smp.h"

extern char __per_cpu_start[];
extern char __per_cpu_end[];
extern unsigned long __per_cpu_offset[CONFIG_SMP_CPU_NUM];

#define DEFINE_PER_CPU(type, name) \
    __typeof__(type) per_cpu__##name __attribute__((section(".bss.percpu")))

#define this_cpu(var) \
    ((void *)((unsigned long)&per_cpu__##var - (unsigned long)__per_cpu_start \
              + __per_cpu_offset[cpu_id()]))

void init_percpu_area(void);
