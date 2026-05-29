#include "io.h"
#include "sys_reg.h"
#include "arch_barrier.h"
#include "gic_ops.h"
#include "gicv3.h"
#include "gicv3_atf.h"
#include "gicv3_private.h"
#include "emul_gic.h"
#include "hyper_config.h"

void init_gicv2(void *gicd_base, void *gicc_base) {
    // hyper_info("gic probe: typer:%x", readl(gicd_base + 0x8));
    u32 *gicd_ctl = (u32 *)gicd_base;
    u32 *gicc_ctl = (u32 *)gicc_base;
    *gicd_ctl |= 1;
    *gicc_ctl |= 1;


    writel(gicc_base + GICC_PMR, 0xff);
    writel(gicd_base + GICD_ISENABLER, 0xffff0000);
}

int gicv3_init_el3(uintptr_t gicr_base) {
    safe_printf("init_el3\n");
    wakeup_gic(gicr_base);
    msr(ICC_SRE_EL3, mrs(ICC_SRE_EL3) | 0xf);

    gicr_write_igroupr0(gicr_base, ~0u);
    gicr_write_igroupr0(gicr_base + 4, ~0u);

    return 0;
}

void early_delay(volatile int cnt) {
    volatile int i = 0;
    while (cnt--)
        i++;
}

void wakeup_gic(uintptr_t gicr_base) {
    u32 count = 100;


    writel((void *)(gicr_base + GICR_WAKER), ~(1u << 1));

    while (readl((void *)(gicr_base + GICR_WAKER)) & GICR_WAKER_ChildrenAsleep) {
        count--;
        if (!count) {
            safe_printf("wakeup timeout\n");
            return;
        }

        early_delay(1000);
    }
}

/*
 * System Register Enable (SRE). Enable to access CPU & Virtual
 * interface registers as system registers in EL2
 */

void enable_sre_el2() {
    u32 val = mrs_s(ICC_SRE_EL2);


    if (!(val & ICC_SRE_SRE_BIT)) {
        val |= ICC_SRE_SRE_BIT;
        val &= ~ICC_SRE_EN_BIT;
        msr_s(ICC_SRE_EL2, val);
        isb();
    }
}

int gicv3_cpu_init(void *gicc_base) {
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
    // arch_gic_write_eoir(id);
    // arch_gic_write_dir(id);
}

int gicv3_rd_init(void *gicr_base) {
    /* enable 5 timer intd + vcpu maintenance intd */
    gicr_write_isenabler0((uintptr_t)gicr_base, 0x7e000000);
    gicr_write_isenabler0((uintptr_t)gicr_base + 4, ~0x0u);


#if 0
    /* Configure SGIs/PPIs as non-secure Group-1 */
    gicr_write_igroupr0(gicr_base, ~0u);
    gicr_write_igroupr0(gicr_base + 4, ~0u);
#endif
    return 0;
}

static void gicv3_enable_host_irq(void *gicd_base, unsigned int intid) {
    uintptr_t base = (uintptr_t)gicd_base;
    unsigned int bit = intid & 31U;
    uintptr_t group_reg = base + GICD_IGROUPR + ((intid >> 5) << 2);
    uintptr_t router_reg = base + GICD_IROUTER + (((intid - 32U) & 0x3ffU) << 3);
    u64 mpidr = mrs(mpidr_el1) & 0xff00ffffffUL;

    mmio_write_32(group_reg, mmio_read_32(group_reg) | (1U << bit));
    mmio_write_64(router_reg, mpidr);
    gicd_set_icpendr(base, intid);
    gicd_set_icactiver(base, intid);
    gicd_set_ipriorityr(base, intid, 0xa0);
    gicd_set_isenabler(base, intid);
}

void gicv3_dist_init(void *gicd_base) {
    safe_printf("GICv3 dist init\n");

    gicd_write_ctlr((uintptr_t)gicd_base,
                    GICD_CTLR_ENABLE_G1 | GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A |
                        GICD_CTLR_ENABLE_G1);

    gicd_set_isenabler((uintptr_t)gicd_base, hyper_config()->timer.hyp_timer_ppi);
    gicd_set_isenabler((uintptr_t)gicd_base, 30);

    if (hyper_config()->uart.enabled)
        gicv3_enable_host_irq(gicd_base, hyper_config()->uart.host_irq);
}

void gicv3_reenable_hyp_timer_ppi(void) {
    mmio_write_32(GICR_SGI_BASE_FIXMAP + 0x100, 1u << hyper_config()->timer.hyp_timer_ppi);
}

void init_gicv3(void *gicd_base, void *gicc_base, void *gicr_base) {
    // hyper_info("gic probe(dist): id:%x, typer:%lx", readl(gicd_base + GICD_IIDR), readl(gicd_base
    // + GICD_TYPER));

    (void)readl(gicd_base + GICD_CTLR);
    // hyper_info("ctlr: %x, pwrr: %x", val, readl(gicr_base + 0x24));
    if (current_el() == 2)
        enable_sre_el2();

    gicv3_dist_init(gicd_base);
    gicv3_rd_init(gicr_base);
    gicv3_cpu_init(gicc_base);
}

/*
https://github.com/seL4/seL4/blob/master/src/arch/arm/machine/gic_v3.c
https://gitlab.arm.com/arm-reference-solutions/arm-reference-solutions-docs/-/blob/master/docs/aemfvp-a/user-guide.rst
https://learn.arm.com/learning-paths/embedded-systems/docker/dockerfile/
*/
