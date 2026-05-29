#include "vmio.h"
#include "emulate.h"
#include "arch_regs.h"
#include "riscv64_system.h"
#include "inline_asm.h"
#include "exception.h"
#include "htypes.h"


int vcpu_redirect_trap(struct cpu_user_regs *regs, struct cpu_vcpu_trap *trap) {

    u64 vsstatus = csrr(CSR_VSSTATUS);

    /* Change Guest SSTATUS.SPP bit */
    vsstatus &= ~SSTATUS_SPP;
    if (regs->sstatus & SSTATUS_SPP)
        vsstatus |= SSTATUS_SPP;

    /* Change Guest SSTATUS.SPIE bit */
    vsstatus &= ~SSTATUS_SPIE;
    if (regs->sstatus & SSTATUS_SIE)
        vsstatus |= SSTATUS_SPIE;

    /* Clear Guest SSTATUS.SIE bit */
    vsstatus &= ~SSTATUS_SIE;

    vsstatus |= (SSTATUS_SPP | SSTATUS_SPIE | SSTATUS_SIE);

    /* Update Guest SSTATUS */
    csrw(CSR_VSSTATUS, vsstatus);

    /* Update Guest SCAUSE, STVAL, and SEPC */
    csrw(CSR_VSCAUSE, trap->scause);
    csrw(CSR_VSTVAL, trap->stval);
    csrw(CSR_VSEPC, trap->sepc);

    /* Set Guest PC to Guest exception vector */
    regs->sepc = csrr(CSR_VSTVEC);

    return 0;
}

int inject_illegal_inst(struct cpu_user_regs *regs, uint64_t inst) {
    struct cpu_vcpu_trap trap;

    /* Redirect trap to Guest VCPU */
    trap.sepc = regs->sepc;
    trap.scause = RISCV_EXCP_ILLEGAL_INST;
    trap.stval = inst;
    return vcpu_redirect_trap(regs, &trap);
}

