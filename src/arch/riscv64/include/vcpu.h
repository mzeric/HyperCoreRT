#pragma once
#include "htypes.h"
#include "arch_regs.h"
#include "list.h"
#include "arch_page.h"

struct arch_vcpu {
    uint64_t stack;
    uint64_t vmpidr;   /* only for aarch64 compat, unused on riscv */
};

struct cpu_arch {
    uint64_t hstatus;
    uint64_t hie;
    uint64_t hip;
    uint64_t hvip;

    /* VS-mode CSR state */
    uint64_t vsstatus;
    uint64_t vstvec;
    uint64_t vsscratch;
    uint64_t vsepc;
    uint64_t vscause;
    uint64_t vstval;
    uint64_t vsatp;
    uint64_t scounteren;

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
