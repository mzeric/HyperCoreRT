#pragma once

#include "config.h"
#include "htypes.h"

#define HYPER_MAX_VCPUS CONFIG_SMP_CPU_NUM

struct hyper_mem_config {
    u64 base;
    u64 size;
};

struct hyper_gic_config {
    u64 gicd_base;
    u64 gicd_size;
    u64 gicr_base;
    u64 gicr_size;
    u64 gicr_stride;
    u32 gicr_count;
};

struct hyper_timer_config {
    u32 hyp_timer_ppi;
    u32 guest_virt_timer_ppi;
};

struct hyper_uart_config {
    u64 host_base;
    u64 host_size;
    u32 host_irq;
    u64 guest_base;
    u64 guest_size;
    u32 guest_irq;
    u32 enabled;
};

struct hyper_guest_vgic_config {
    u64 gicd_base;
    u64 gicd_size;
    u64 gicr_base;
    u64 gicr_size;
    u64 gicr_stride;
};

struct hyper_guest_config {
    u64 entry;
    u64 dtb_addr;
    struct hyper_mem_config memory;
    u32 vcpu_count;
    u64 vcpu_mpidr[HYPER_MAX_VCPUS];
    struct hyper_guest_vgic_config vgic;
};

struct hyper_config {
    struct hyper_mem_config memory;
    struct hyper_gic_config host_gic;
    struct hyper_timer_config timer;
    struct hyper_guest_config guest;
    struct hyper_uart_config uart;
};

extern struct hyper_config g_hyper_config;

struct hyper_config *hyper_config(void);
