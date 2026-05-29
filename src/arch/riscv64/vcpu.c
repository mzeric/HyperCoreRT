#include "vcpu.h"
#include "kmalloc.h"
#include <string.h>

#define VCPU_STACK_SIZE (4096)

int arch_vcpu_reset(vcpu_t *vcpu) {
    int rc = 0;
    // vcpu->carch.hvip
    return rc;
}

vcpu_t *create_vcpu(int vcpu_d, int priority) {
    vcpu_t *vcpu = NULL;

    if (!(vcpu = kmalloc(sizeof(vcpu_t)))) {
        hyper_fatal("alloc vcpu struct failed\n");
        return NULL;
    }

    memset(vcpu, 0, sizeof(vcpu_t));

    INIT_LIST_HEAD(&vcpu->list);

    vcpu->priority = priority;

    /* read-write lock here */

    /* need tasklet here */

    vcpu->arch.stack = kmalloc(VCPU_STACK_SIZE) + VCPU_STACK_SIZE;
    // vcpu->arch.saved_context.sp =
    arch_vcpu_reset(vcpu);

    return vcpu;
}
#include "emulate.h"
void vcpu_context_restore(vcpu_t *vcpu) {
    // msr(sp_el1, vcpu->arch.stack);
    // msr(spsr_el1, 0);
    // msr(cpacr_el1, vcpu->arch.cpacr);


    // msr(sctlr_el1, vcpu->arch.sctlr_el1);
    u64 vs = 0;

    csrw(CSR_HSTATUS, vcpu->carch.hstatus);

    csrw(CSR_HIE, (1 << 2 | 1 << 6 | 1 << 10 | 1 << 12));

    // inject_illegal_inst(&vcpu->regs, 0x1023);
    // safe_printf("inject pc:%lx\n", vcpu->regs.sepc);

    // vs |= (1u<<8);
    // csrw(CSR_VSSTATUS, vs);

    // csrw(CSR_HIDELEG, (1u << 1) | (1u << 5) | (1u << 9));
    csrw(CSR_HIDELEG, (1u << 2) | (1u << 6) | (1u << 10));

    // csrw(CSR_HVIP, (1 << 6));
    // csrw(CSR_HIP, (1u<<2));
    safe_printf("debug: hedeleg %lx\n", csrr(CSR_HIDELEG));
}

void vcpu_context_save(vcpu_t *vcpu) {
    vcpu->arch.stack = 0;
    vcpu->carch.hstatus = csrr(CSR_HSTATUS);
}

#define HSTATUS_VS (HSTATUS_SPV | HSTATUS_SPVP)
#define HSTATUS_VU (HSTATUS_SPV)
#define HSTATUS_HS (HSTATUS_SPVP)

int arch_vcpu_init(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack) {
    vcpu->arch.stack = stack;
    vcpu->regs.sepc = entry;
    vcpu->regs.ra = entry;

    vcpu->carch.hstatus = HSTATUS_VS;//switch to vs
}
