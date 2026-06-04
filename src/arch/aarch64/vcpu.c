#include <string.h>
#include "vcpu.h"
#include "mm.h"
#include "aarch64_hcr.h"
#include "aarch64_system.h"
#include "arch_barrier.h"
#include <ioremap.h>
#include <guest_memory.h>
#include <emul_dev.h>
#include <config.h>
#include "sys_reg.h"
#include "src/drivers/gic/gicv3.h"
#include "hyper_config.h"

/* Numeric encodings for msr_s/mrs_s (the tokenized ICC_* in gicv3.h
   don't expand to integers that msr_s/sys_reg() accepts). */
#define ENC_ICC_PMR_EL1     sys_reg(3, 0, 4, 6, 0)
#define ENC_ICC_BPR1_EL1    sys_reg(3, 0, 12, 12, 3)
#define ENC_ICC_CTLR_EL1    sys_reg(3, 0, 12, 12, 4)
#define ENC_ICC_SRE_EL1     sys_reg(3, 0, 12, 12, 5)
#define ENC_ICC_IGRPEN1_EL1 sys_reg(3, 0, 12, 12, 7)

#define SCTLR_VCPU_DEFAULT                                                                              \
    (SCTLR_EL1_RES1 | SCTLR_EL1_UCI_DIS | SCTLR_EL1_EE_LE | SCTLR_EL1_WXN_DIS |                    \
     SCTLR_EL1_NTWE_DIS | SCTLR_EL1_NTWI_DIS | SCTLR_EL1_UCT_DIS | SCTLR_EL1_DZE_DIS |             \
     SCTLR_EL1_ICACHE_DIS | SCTLR_EL1_UMA_DIS | SCTLR_EL1_SED_EN | SCTLR_EL1_ITD_EN |              \
     SCTLR_EL1_CP15BEN_DIS | SCTLR_EL1_SA0_DIS | SCTLR_EL1_SA_DIS | SCTLR_EL1_DCACHE_DIS |         \
     SCTLR_EL1_ALIGN_DIS | SCTLR_EL1_MMU_DIS)

#define SPSR_EL2_VCPU_DEFAULT                                                                      \
    (SPSR_EL_DEBUG_MASK | SPSR_EL_SERR_MASK | SPSR_EL_IRQ_MASK | SPSR_EL_FIQ_MASK |                \
     SPSR_EL_M_AARCH64 | SPSR_EL_M_EL1H)

static void vcpu_fpsimd_save(vcpu_t *vcpu)
{
    u64 fpsr;
    u64 fpcr;
    u64 *q = &vcpu->arch.fpsimd.q[0][0];

    asm volatile(
        "stp q0, q1, [%2, #0]\n"
        "stp q2, q3, [%2, #32]\n"
        "stp q4, q5, [%2, #64]\n"
        "stp q6, q7, [%2, #96]\n"
        "stp q8, q9, [%2, #128]\n"
        "stp q10, q11, [%2, #160]\n"
        "stp q12, q13, [%2, #192]\n"
        "stp q14, q15, [%2, #224]\n"
        "stp q16, q17, [%2, #256]\n"
        "stp q18, q19, [%2, #288]\n"
        "stp q20, q21, [%2, #320]\n"
        "stp q22, q23, [%2, #352]\n"
        "stp q24, q25, [%2, #384]\n"
        "stp q26, q27, [%2, #416]\n"
        "stp q28, q29, [%2, #448]\n"
        "stp q30, q31, [%2, #480]\n"
        "mrs %0, fpsr\n"
        "mrs %1, fpcr\n"
        : "=r"(fpsr), "=r"(fpcr)
        : "r"(q)
        : "memory");

    vcpu->arch.fpsimd.fpsr = fpsr;
    vcpu->arch.fpsimd.fpcr = fpcr;
}

static void vcpu_fpsimd_restore(vcpu_t *vcpu)
{
    u64 *q = &vcpu->arch.fpsimd.q[0][0];

    asm volatile(
        "ldp q0, q1, [%0, #0]\n"
        "ldp q2, q3, [%0, #32]\n"
        "ldp q4, q5, [%0, #64]\n"
        "ldp q6, q7, [%0, #96]\n"
        "ldp q8, q9, [%0, #128]\n"
        "ldp q10, q11, [%0, #160]\n"
        "ldp q12, q13, [%0, #192]\n"
        "ldp q14, q15, [%0, #224]\n"
        "ldp q16, q17, [%0, #256]\n"
        "ldp q18, q19, [%0, #288]\n"
        "ldp q20, q21, [%0, #320]\n"
        "ldp q22, q23, [%0, #352]\n"
        "ldp q24, q25, [%0, #384]\n"
        "ldp q26, q27, [%0, #416]\n"
        "ldp q28, q29, [%0, #448]\n"
        "ldp q30, q31, [%0, #480]\n"
        "msr fpsr, %1\n"
        "msr fpcr, %2\n"
        :: "r"(q), "r"(vcpu->arch.fpsimd.fpsr), "r"(vcpu->arch.fpsimd.fpcr)
        : "memory");
}

struct mem_region *new_mem_regon(const char *name, u64 gpa, u64 hpa, u64 size, int attr) {
    struct mem_region *region = (struct mem_region *)kmalloc(sizeof(struct mem_region));
    memset(region, 0, sizeof(struct mem_region));
    region->gpa = gpa;
    region->hpa = hpa;
    region->size = size;
    region->attr = attr;

    if (name) {
        strncpy(region->match_name, name, sizeof(region->match_name) - 1);
        region->match_name[sizeof(region->match_name) - 1] = '\0';
    }

    probe_emul_dev(region);

    return region;
}

#include "arch_page.h"
struct stage2_mm_info *get_default_mm_info();

/* only touch vcpu->regs (EL2 regs) in init */
int arch_vcpu_init(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack) {
    vcpu->regs.pc = entry;
    vcpu->regs.sp = ~0ul; /* guest's sp_el2 */
    vcpu->regs.lr = vcpu->regs.pc; /* return to reset */
    vcpu->regs.cpsr = SPSR_EL2_VCPU_DEFAULT;

    vcpu->arch.stack = (void *)stack;

    INIT_LIST_HEAD(&vcpu->mem_region);

#ifdef CONFIG_BOARD_QEMU_VIRT

    struct hyper_config *cfg = hyper_config();
    u64 gic_emul_base = cfg->guest.vgic.gicd_base;
    u64 gic_emul_end = cfg->guest.vgic.gicr_base + cfg->guest.vgic.gicr_size;
    struct mem_region *region = new_mem_regon("gic3", gic_emul_base, gic_emul_base, gic_emul_end - gic_emul_base, MEM_ACCESS_NONE);
    struct mem_region *region2 = new_mem_regon("pl011", cfg->uart.guest_base, cfg->uart.guest_base, cfg->uart.guest_size, MEM_ACCESS_NONE);
    struct mem_region *region3 = new_mem_regon(NULL, cfg->guest.memory.base, cfg->guest.memory.base, cfg->guest.memory.size, MEM_ACCESS_RWX);

    guest_mem_add_region(vcpu, region);
    guest_mem_add_region(vcpu, region2);
    guest_mem_add_region(vcpu, region3);

#else
    struct mem_region *region = new_mem_regon(NULL, 0x90000000, 0x90000000, 0x01000000, MEM_ACCESS_RWX);
    struct mem_region *region2 = new_mem_regon("pl011", 0x50000000, 0x1c090000, 0x00100000, MEM_ACCESS_NONE);
    struct mem_region *region3 = new_mem_regon(NULL, 0x40000000, 0x90000000, 0x10000000, MEM_ACCESS_NONE);


    guest_mem_add_region(vcpu, region);
    guest_mem_add_region(vcpu, region2);
    guest_mem_add_region(vcpu, region3);
#endif

    /* setup stage-2 translation */
    vcpu->mm_info = *get_default_mm_info();


    return 0;

}

int arch_vcpu_reset(vcpu_t *vcpu) {
    int rc = 0;

    vcpu->arch.sctlr_el1 = SCTLR_VCPU_DEFAULT;
    vcpu->arch.cpacr = 3u << 20;


    return rc;
}

void vcpu_context_save(vcpu_t *vcpu) {

    vcpu->arch.csselr = mrs(CSSELR_EL1);
    vcpu_fpsimd_save(vcpu);

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

    /* Physical timer (EL1 view). Each vCPU may program its own physical
       timer; without save/restore the last writer's state leaks into the
       next vCPU. */
    vcpu->arch.phys_timer.ctlr = mrs(cntp_ctl_el0);
    vcpu->arch.phys_timer.cval = mrs(cntp_cval_el0);

    /* PAR_EL1 holds the result of the guest's AT (address translate)
       instructions.  If the next vCPU also executes AT before this one
       reads PAR, the value is silently clobbered. */
    vcpu->arch.par = mrs(par_el1);

    /* Save GICv3 CPU interface (only the EL1 regs - SRE_EL2 / HCR_EL2 are
       global per-pCPU). */
    vcpu->arch.gicc.pmr     = mrs_s(ENC_ICC_PMR_EL1);
    vcpu->arch.gicc.ctlr    = mrs_s(ENC_ICC_CTLR_EL1);
    vcpu->arch.gicc.igrpen1 = mrs_s(ENC_ICC_IGRPEN1_EL1);
    vcpu->arch.gicc.bpr1    = mrs_s(ENC_ICC_BPR1_EL1);
    vcpu->arch.gicc.sre_el1 = mrs_s(ENC_ICC_SRE_EL1);
    vcpu->arch.gicc.initialized = true;

    isb();
    /* MMU */

    vcpu->arch.sctlr_el1 = mrs(sctlr_el1);
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

    vcpu->arch.spsr_el1 = mrs(spsr_el1);
    vcpu->arch.elr_el1  = mrs(elr_el1);
    vcpu->arch.sp_el0  = mrs(sp_el0);
    vcpu->arch.stack   = (void *)mrs(sp_el1);

    // FOR linux dtb
    //vcpu->regs.x0 = 0x90000000;

    isb();
}

void vcpu_context_restore(vcpu_t *vcpu) {
    /* Clear the local exclusive monitor on vCPU switch so a stale LDXR
       from the previous vCPU cannot succeed on a subsequent STXR. */
    asm volatile("clrex" ::: "memory");

    vcpu_fpsimd_restore(vcpu);
    msr(spsr_el1, vcpu->arch.spsr_el1);
    msr(elr_el1, vcpu->arch.elr_el1);
    msr(sp_el0, vcpu->arch.sp_el0);
    msr(sp_el1, vcpu->arch.stack);
    msr(cpacr_el1, vcpu->arch.cpacr);

    msr(sctlr_el1, vcpu->arch.sctlr_el1);

    /* Restore per-vCPU virtual timer programming. Each vcpu has its own
       cntv_cval/ctlr; otherwise multiple vCPUs scribble over the single
       physical CPU's virt-timer state. */
    msr(cntv_cval_el0, vcpu->arch.virt_timer.cval);
    msr(cntv_ctl_el0,  vcpu->arch.virt_timer.ctlr);

    /* Restore per-vCPU physical timer state. */
    msr(cntp_cval_el0, vcpu->arch.phys_timer.cval);
    msr(cntp_ctl_el0,  vcpu->arch.phys_timer.ctlr);

    /* Restore PAR_EL1 so AT instruction results survive across switches. */
    msr(par_el1,       vcpu->arch.par);

    /* Hand the guest its own MPIDR. Linux requires secondary cores to read
       distinct MPIDRs (matching dts cpu@N/reg) otherwise check_local_cpu_capabilities
       panics. Use the per-vcpu mpidr value (assumed valid on first switch). */
    msr(vmpidr_el2, vcpu->arch.vmpidr);
    msr(vpidr_el2,  mrs(midr_el1));

    /* Restore the remaining per-vCPU EL1 state that vcpu_context_save read.
       Without this, two vcpus on the same physical CPU stomp on each other's
       translation, exception-vector, per-task base, and timer config. */
    msr(ttbr0_el1,    vcpu->arch.ttbr0);
    msr(ttbr1_el1,    vcpu->arch.ttbr1);
    msr(tcr_el1,      vcpu->arch.ttbcr);
    /* TTBR0 carries the guest ASID; a vCPU switch can change the ASID,
       so we must invalidate stale Stage-1 TLB entries before ERET. */
    asm volatile("tlbi vmalle1is");
    asm volatile("tlbi vmalls12e1is");
    asm volatile("dsb ish");
    asm volatile("isb");
    msr(mair_el1,     vcpu->arch.mair);
    msr(amair_el1,    vcpu->arch.amair);
    msr(vbar_el1,     vcpu->arch.vbar);
    msr(contextidr_el1, vcpu->arch.contextidr);
    msr(tpidr_el0,    vcpu->arch.tpidr_el0);
    msr(tpidrro_el0,  vcpu->arch.tpidrro_el0);
    msr(tpidr_el1,    vcpu->arch.tpidr_el1);
    msr(cntkctl_el1,  vcpu->arch.cntkctl);

    /* Restore exception-status registers so a late-arriving timer IRQ
       cannot leave the resuming vCPU reading the other vCPU's FAR/ESR. */
    msr(far_el1,      vcpu->arch.far);
    msr(esr_el1,      vcpu->arch.esr);
    msr(afsr0_el1,    vcpu->arch.afsr0);
    msr(afsr1_el1,    vcpu->arch.afsr1);
    msr(csselr_el1,   vcpu->arch.csselr);

    /* Restore the vcpu's view of the GICv3 CPU interface, if it has ever
       configured one. Otherwise leave hyp's defaults (set in gicv3_cpu_init). */
    if (vcpu->arch.gicc.initialized) {
        msr_s(ENC_ICC_PMR_EL1,     vcpu->arch.gicc.pmr);
        msr_s(ENC_ICC_CTLR_EL1,    vcpu->arch.gicc.ctlr);
        msr_s(ENC_ICC_BPR1_EL1,    vcpu->arch.gicc.bpr1);
        msr_s(ENC_ICC_IGRPEN1_EL1, vcpu->arch.gicc.igrpen1);
        msr_s(ENC_ICC_SRE_EL1,     vcpu->arch.gicc.sre_el1);
        isb();
    }
}

void test_vcpu() {
    vcpu_t *vcpu = create_vcpu(1, 100);
    vcpu_context_save(vcpu);

}

