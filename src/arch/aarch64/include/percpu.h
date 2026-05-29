#pragma once
#include "config.h"
#include "aarch64_system.h"

extern char __per_cpu_start[], __per_cpu_data_end[];
extern unsigned long __per_cpu_offset[CONFIG_SMP_CPU_NUM];

#define __DEFINE_PER_CPU(attr, type, name) attr __typeof__(type) per_cpu_##name

#define DEFINE_PER_CPU(type, name) __DEFINE_PER_CPU(__section(".bss.percpu"), type, _##name)

#define DECLARE_PER_CPU(type, name) extern __typeof__(type) per_cpu__##name


#define this_cpu(var) (((void *)&per_cpu__##var - __per_cpu_start) + __per_cpu_offset[smp_id()])
