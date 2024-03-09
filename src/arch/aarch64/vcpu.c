#include <string.h>
#include "vcpu.h"
#include "mm.h"
#include "cpu_aarch64.h"
#include "system.h"
#include "arch_barrier.h"

#define SCTLR_VCPU_DEFAULT                                                                              \
    (SCTLR_EL1_RES1 | SCTLR_EL1_UCI_DIS | SCTLR_EL1_EE_LE | SCTLR_EL1_WXN_DIS |                    \
     SCTLR_EL1_NTWE_DIS | SCTLR_EL1_NTWI_DIS | SCTLR_EL1_UCT_DIS | SCTLR_EL1_DZE_DIS |             \
     SCTLR_EL1_ICACHE_DIS | SCTLR_EL1_UMA_DIS | SCTLR_EL1_SED_EN | SCTLR_EL1_ITD_EN |              \
     SCTLR_EL1_CP15BEN_DIS | SCTLR_EL1_SA0_DIS | SCTLR_EL1_SA_DIS | SCTLR_EL1_DCACHE_DIS |         \
     SCTLR_EL1_ALIGN_DIS | SCTLR_EL1_MMU_DIS)

#define SPSR_EL2_VCPU_DEFAULT                                                                      \
    (SPSR_EL_DEBUG_MASK | SPSR_EL_SERR_MASK | SPSR_EL_IRQ_MASK | SPSR_EL_FIQ_MASK |                \
     SPSR_EL_M_AARCH64 | SPSR_EL_M_EL1H)

/* only touch vcpu->regs (EL2 regs) in init */
int arch_vcpu_init(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack) {
    vcpu->regs.pc = entry;
    vcpu->regs.sp = ~0ul; /* guest's sp_el2 */
    vcpu->regs.lr = vcpu->regs.pc; /* return to reset */
    vcpu->regs.cpsr = SPSR_EL2_VCPU_DEFAULT;

    vcpu->arch.stack = stack;
}

int arch_vcpu_reset(vcpu_t *vcpu) {
    int rc = 0;

    vcpu->arch.sctlr_el1 = SCTLR_VCPU_DEFAULT;
    vcpu->arch.cpacr = 3u << 20;


    return rc;
}

void destroy_vcpu(vcpu_t *vcpu) {
    if(vcpu->arch.saved_context.sp)
        kfree(vcpu->arch.saved_context.sp);


    kfree(vcpu);
}
/*
xvisor: vmm_manager_vcpu_orphan_create
xen: vcpu_create

arch_vcpu_create
*/
#define VCPU_STACK_SIZE (4096)
vcpu_t *create_vcpu(int vcpu_d, int priority) {

    vcpu_t *vcpu = NULL;

    if(!(vcpu = kmalloc(sizeof(vcpu_t)))){
        vmm_fatal("alloc vcpu struct failed\n");
        return NULL;
    }

    memset(vcpu, 0, sizeof(vcpu_t));

    INIT_LIST_HEAD(&vcpu->list);

    vcpu->priority = priority;

    /* read-write lock here */

    /* need tasklet here */

    vcpu->arch.stack = kmalloc(VCPU_STACK_SIZE) + VCPU_STACK_SIZE;
    //vcpu->arch.saved_context.sp =
    arch_vcpu_reset(vcpu);

    return vcpu;
}

void vcpu_context_save(vcpu_t *vcpu) {

    vcpu->arch.csselr = mrs(CSSELR_EL1);
    /* TODO: VFP */

    /* control register */
    vcpu->arch.cpacr = mrs(cpacr_EL1);
    vcpu->arch.contextidr = mrs(ContextIDR_EL1);
    vcpu->arch.tpidr_el0 = mrs(tpidr_EL0);
    vcpu->arch.tpidrro_el0 = mrs(tpidrro_EL0);
    vcpu->arch.tpidr_el1 = mrs(tpidr_EL1);

    /* timer */
    vcpu->arch.cntkctl = mrs(cntkctl_el1);

    vcpu->arch.virt_timer.ctlr = mrs(cntv_ctl_el0);

    /* disable vtimer */
    msr(cntv_ctl_el0, (vcpu->arch.virt_timer.ctlr & ~CNTx_CTL_ENABLE));

    vcpu->arch.virt_timer.cval = mrs(cntv_cval_el0);

    if ((vcpu->arch.virt_timer.ctlr & CNTx_CTL_ENABLE) &&
        !(vcpu->arch.virt_timer.ctlr & CNTx_CTL_MASK)) {
#if 0
        set_timer(&v->arch.virt_timer.timer,
                  v->domain->arch.virt_timer_base.nanoseconds +
                  ticks_to_ns(v->arch.virt_timer.cval));
#endif
    }

    isb();
    /* MMU */

    vcpu->arch.vbar = mrs(vbar_EL1);
    vcpu->arch.ttbcr = mrs(tcr_EL1);
    vcpu->arch.ttbr0 = mrs(ttbr0_EL1);
    vcpu->arch.ttbr1 = mrs(ttbr1_EL1);
    vcpu->arch.mair = mrs(mair_EL1);
    vcpu->arch.amair = mrs(amair_EL1);


    /* excep status */
    vcpu->arch.far = mrs(far_EL1);
    vcpu->arch.esr = mrs(esr_EL1);


    vcpu->arch.afsr0 = mrs(afsr0_EL1);
    vcpu->arch.afsr1 = mrs(afsr1_EL1);

    /* stack */

    vcpu->arch.stack = mrs(sp_el1);

    isb();
}

void vcpu_context_restore(vcpu_t *vcpu) {
    msr(sp_el1, vcpu->arch.stack);
    msr(spsr_el1, 0);
    msr(cpacr_el1, vcpu->arch.cpacr);


    msr(sctlr_el1, vcpu->arch.sctlr_el1);
}

void test_vcpu() {
    vcpu_t *vcpu = create_vcpu(1, 100);
    vcpu_context_save(vcpu);

}
