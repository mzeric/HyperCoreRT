#pragma once
#include "htypes.h"
#include "arch_regs.h"
#include "list.h"
#include "arch_page.h"
#include "spin_lock.h"

struct arch_vcpu {
    uint64_t stack;
    uint64_t vmpidr;   /* only for aarch64 compat, unused on riscv */
};

struct riscv_vcpu_timer_state {
    uint64_t deadline_cycles;
    uint8_t armed;
    uint8_t pending;
};

struct cpu_arch {
    uint64_t hstatus;
    uint64_t hie;
    uint64_t hip;
    uint64_t virt_irq_pending;
    spinlock_t virt_irq_lock;

    /* VS-mode CSR state */
    uint64_t vsstatus;
    uint64_t vsie;
    uint64_t vstvec;
    uint64_t vsscratch;
    uint64_t vsepc;
    uint64_t vscause;
    uint64_t vstval;
    uint64_t vsatp;
    uint64_t scounteren;

    uint64_t f[32];
    uint64_t fcsr;

    uint64_t vstart;
    uint64_t vxsat;
    uint64_t vxrm;
    uint64_t vcsr;
    uint64_t vl;
    uint64_t vtype;
    uint64_t vlenb;
    void    *vregs;
    uint64_t vregs_size;

    riscv_vcpu_timer_state timer;

    void *timer_priv;
};

typedef struct vcpu {
    int vcpu_id;
    int phys_id;
    int priority;
    struct list_head     list;

    struct arch_vcpu     arch;
    struct cpu_arch      carch;
    struct cpu_user_regs regs;

    struct list_head mem_region;

    struct stage2_mm_info mm_info;
} vcpu_t;

vcpu_t *create_vcpu(int vcpu_id, int priority);
void    destroy_vcpu(vcpu_t *vcpu);
int     arch_vcpu_init(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack);
void    vcpu_context_save(vcpu_t *vcpu);
void    vcpu_context_restore(vcpu_t *vcpu);
