#include "gicv3.h"
#include "io.h"
#include "sys_reg.h"

void init_gicv2(void *gicd_base, void *gicc_base) {
    // vmm_info("gic probe: typer:%x\n", readl(gicd_base + 0x8));
    u32 *gicd_ctl = (u32 *)gicd_base;
    u32 *gicc_ctl = (u32 *)gicc_base;
    *gicd_ctl |= 1;
    *gicc_ctl |= 1;

    writel(gicc_base + GICC_PMR, 0xff);
    writel(gicd_base + GICD_ISENABLER, 0xffff0000);
}

/*
 * System Register Enable (SRE). Enable to access CPU & Virtual
 * interface registers as system registers in EL2
 */
static void gicv3_enable_sre(void) {
    int val;

    val = mrs_s(ICC_SRE_EL2);
    if (val & GICC_SRE_EL2_SRE)
        return;

    val |= GICC_SRE_EL2_SRE;
    val &= ~(1u << 3);

    msr_s(ICC_SRE_EL2, val);
    isb();
}

void enable_sre_el2() {
    u32 val = mrs_s(ICC_SRE_EL2);


    // while(1);
    if (!(val & GICC_SRE_EL2_SRE)) {
        val |= GICC_SRE_EL2_SRE;
        val &= ~GICC_SRE_EL2_ENEL1;
        msr_s(ICC_SRE_EL2, val);
        isb();
    }

}

int gicv3_cpu_init(void *gicc_base) {
    // vmm_info("GICv3 cpu interface init\n");

    // msr(ICC_BPR1_EL1, 0);

    msr(ICC_PMR_EL1, 0xff);
    // enable EoI mode
    msr(ICC_CTLR_EL1, (1u << 1));
    msr(ICC_IGRPEN1_EL1, 1);

    asm("isb" ::: "memory");

    return 0;
}

void gicv3_eof_int(int id) {
    msr(ICC_EOIR1_EL1, id);
    msr(ICC_DIR_EL1, id);
}
int gicv3_rd_init(void *gicr_base) {
    int id = 30;
#if 0
    vmm_info("GICv3 re-dist init, id: %x, typer: %x\n", readl(gicr_base + GICR_IIDR), readl(gicr_base + GICR_TYPER));
    vmm_debug("GICv3 re-dist pid: %x %x %x %x, %x %x %x %x\n",
              readl(gicr_base + 0xFFD0),
              readl(gicr_base + 0xFFD4),
              readl(gicr_base + 0xFFD8),
              readl(gicr_base + 0xFFDC),
              readl(gicr_base + 0xFFE0),
              readl(gicr_base + 0xFFE4),
              readl(gicr_base + 0xFFE8),
              readl(gicr_base + 0xFFEC));

    vmm_info("hack: %x\n", readl(gicr_base + GICR_CTLR));
#endif
    /* enable 5 timer intd + vcpu maintenance intd */
    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_ISENABLER0, 0x7e000000);
    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_ISENABLER0 + 4, ~0x0u);

    /* Configure SGIs/PPIs as non-secure Group-1 */
    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_IGROUPR0, ~0u);
    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_IGROUPR0 + 4, ~0u);

    return 0;
}

void gicv3_dist_init(void *gicd_base) {
    safe_printf("GICv3 dist init\n");
    // vmm_info("GICD_CTRL:%x\n", readl(gicd_base));
    writel(gicd_base + GICD_CTLR,
           GICD_CTL_ENABLE | GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A | GICD_CTLR_ENABLE_G1);

    // vmm_info("GICD_CTRL:%x\n", readl(gicd_base));

    writel(gicd_base + GICD_ISENABLER, 0xffff0000);
}

void init_gicv3(void *gicd_base, void *gicc_base, void *gicr_base) {
    // vmm_info("gic probe(dist): id:%x, typer:%lx\n", readl(gicd_base + GICD_IIDR), readl(gicd_base + GICD_TYPER));

    u32 val;

    val = readl(gicd_base + GICD_CTLR);
    // vmm_info("ctlr: %x, pwrr: %x\n", val, readl(gicr_base + 0x24));
    if (current_el() == 2)
        enable_sre_el2();

    gicv3_dist_init(gicd_base);
    gicv3_rd_init(gicr_base);
    gicv3_cpu_init(gicc_base);

}
/*
https://github.com/seL4/seL4/blob/master/src/arch/arm/machine/gic_v3.c
https://gitlab.arm.com/arm-reference-solutions/arm-reference-solutions-docs/-/blob/master/docs/aemfvp-a/user-guide.rst
*/