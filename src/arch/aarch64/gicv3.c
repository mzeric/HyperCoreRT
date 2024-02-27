#include "gicv3.h"
#include "io.h"

void init_gicv2(void *gicd_base, void *gicc_base) {
    vmm_info("gic probe: typer:%x\n", readl(gicd_base + 0x8));
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

    val = READ_SYSREG(ICC_SRE_EL2);
    if (val & GICC_SRE_EL2_SRE)
        return;

    val |= GICC_SRE_EL2_SRE;
    val &= ~(1u << 3);

    WRITE_SYSREG(val, ICC_SRE_EL2);
    isb();
}

volatile void gic_udelay(int cnt) {
    while (cnt)
        cnt--;
}

static void gic_rd_wait_for_wake(void *rd_base) {
    u32 volatile count = 100;

    while (readl(rd_base + GICR_WAKER) & GICR_WAKER_ChildrenAsleep) {
        count--;
        if (!count) {
            vmm_info("wake timeout\n");
            return;
        }
        gic_udelay(1);
    };
}

int gicv3_cpu_init(void) {
    vmm_info("GICv3 cpu interface init\n");

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
    vmm_info("GICv3 re-dist init\n");

    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_ISENABLER0, 0x40000000);

    /* Configure SGIs/PPIs as non-secure Group-1 */
    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_IGROUPR0, ~0u);

    return 0;
}

void gicv3_dist_init(void *gicd_base) {
    vmm_info("GICv3 dist init\n");
    vmm_info("GICD_CTRL:%x\n", readl(gicd_base));
    writel(gicd_base + GICD_CTLR,
           GICD_CTL_ENABLE | GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A | GICD_CTLR_ENABLE_G1);

    vmm_info("GICD_CTRL:%x\n", readl(gicd_base));

    writel(gicd_base + GICD_ISENABLER, 0xffff0000);
}

void init_gicv3(void *gicd_base, void *gicr_base) {
    vmm_info("gic probe: id:%x, typer:%x\n", readl(gicd_base + 0x4), readl(gicd_base + 0x8));

    // gic_rd_wait_for_wake(gicr_base);

    gicv3_rd_init(gicr_base);
    gicv3_cpu_init();
    gicv3_dist_init(gicd_base);
}