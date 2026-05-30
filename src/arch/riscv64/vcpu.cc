#include "vcpu.h"
#include "kmalloc.h"
#include <string.h>
#include "inline_asm.h"
#include "mmu.h"
#include "exception.h"

#define VCPU_STACK_SIZE (4096)

static void arch_vcpu_reset(vcpu_t *vcpu) {
    /* nothing yet */
}

vcpu_t *create_vcpu(int vcpu_id, int priority) {
    vcpu_t *vcpu = (vcpu_t *)kmalloc(sizeof(vcpu_t));
    if (!vcpu) {
        return NULL;
    }

    memset(vcpu, 0, sizeof(vcpu_t));
    INIT_LIST_HEAD(&vcpu->list);
    vcpu->priority = priority;

    vcpu->arch.stack = (uint64_t)((char *)kmalloc(VCPU_STACK_SIZE) + VCPU_STACK_SIZE);
    arch_vcpu_reset(vcpu);

    return vcpu;
}

void destroy_vcpu(vcpu_t *vcpu) {
    if (vcpu->arch.stack)
        kfree((void *)(vcpu->arch.stack - VCPU_STACK_SIZE));
    kfree(vcpu);
}

#define HSTATUS_VS (HSTATUS_SPV | HSTATUS_SPVP)

int arch_vcpu_init(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack) {
    vcpu->arch.stack = stack;

    /* Set up guest entry in the trap frame */
    vcpu->regs.sepc = entry;
    vcpu->regs.ra = entry;
    vcpu->regs.sp = stack;

    /* Set HSTATUS to enter VS-mode on sret */
    vcpu->carch.hstatus = HSTATUS_VS;

    /* Set VSSTATUS: SPP=0 (return to U-mode within guest), SPIE=1 */
    vcpu->carch.vsstatus = SSTATUS_SPIE;

    return 0;
}

void vcpu_context_save(vcpu_t *vcpu) {
    /* Save H-extension CSRs */
    vcpu->carch.hstatus   = csrr(CSR_HSTATUS);
    vcpu->carch.vsstatus  = csrr(CSR_VSSTATUS);
    vcpu->carch.vsepc     = csrr(CSR_VSEPC);
    vcpu->carch.vstvec    = csrr(CSR_VSTVEC);
    vcpu->carch.vsscratch = csrr(CSR_VSSCRATCH);
    vcpu->carch.vscause   = csrr(CSR_VSCAUSE);
    vcpu->carch.vstval    = csrr(CSR_VSTVAL);
    vcpu->carch.vsatp     = csrr(CSR_VSATP);
    vcpu->carch.hie       = csrr(CSR_HIE);

    /* Save hypervisor interrupt pending */
    vcpu->carch.hvip      = csrr(CSR_HVIP);
}

void vcpu_context_restore(vcpu_t *vcpu) {
    /* Restore VS-mode CSRs */
    csrw(CSR_VSSTATUS,  vcpu->carch.vsstatus);
    csrw(CSR_VSEPC,     vcpu->carch.vsepc);
    csrw(CSR_VSTVEC,    vcpu->carch.vstvec);
    csrw(CSR_VSSCRATCH, vcpu->carch.vsscratch);
    csrw(CSR_VSCAUSE,   vcpu->carch.vscause);
    csrw(CSR_VSTVAL,    vcpu->carch.vstval);
    csrw(CSR_VSATP,     vcpu->carch.vsatp);

    /* Restore hypervisor interrupt configuration */
    csrw(CSR_HIE,  vcpu->carch.hie);
    csrw(CSR_HVIP, vcpu->carch.hvip);

    /* Delegate interrupts to VS-mode: VS-mode timer, external, software */
    csrw(CSR_HIDELEG, (1UL << IRQ_VS_SOFT) |
                      (1UL << IRQ_VS_TIMER) |
                      (1UL << IRQ_VS_EXT));

    /* Set HSTATUS.SPV=1 so sret enters VS-mode */
    csrw(CSR_HSTATUS, vcpu->carch.hstatus);
}
