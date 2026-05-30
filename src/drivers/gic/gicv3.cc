#include "io.h"
#include "sys_reg.h"
#include "arch_barrier.h"
#include "inline_asm.h"
#include "config.h"
#include "gic_ops.h"
#include "gicv3.h"
#include "gicv3_atf.h"
#include "gicv3_private.h"
#include "emul_gic.h"
#include "hyper_config.h"
#include "smp.h"
#include "ipi.h"

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
    uintptr_t base = (uintptr_t)gicr_base;

    /* Configure SGIs/PPIs as non-secure Group-1.
     * Without this, PPIs default to Group 0 and ICC_IGRPEN1_EL1=1
     * (Group-1 only) will never deliver them. */
    gicr_write_igroupr0(base, ~0u);

    /* Clear Group-0 modifier so all SGIs/PPIs are purely Group-1 NS. */
    gicr_write_igrpmodr0(base, 0);

    /* Set default priority 0xa0 for all 32 SGIs/PPIs (8 per register). */
    for (unsigned int i = 0; i < 32; i += 4)
        gicr_ipriorityr_write(base, i / 4, 0xa0a0a0a0u);

    /* Enable SGIs (all 16) and required PPIs (timer, maintenance). */
    gicr_write_isenabler0(base, ~0x0u);
    gicr_write_isenabler0(base, 0x7e000000);

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
    uintptr_t gicr_base = hyper_config()->host_gic.gicr_virt +
                          (uintptr_t)cpu_id() * hyper_config()->host_gic.gicr_stride;
    gicr_write_isenabler0(gicr_base, 1u << hyper_config()->timer.hyp_timer_ppi);
}

void init_gicv3(void *gicd_base, void *gicc_base, void *gicr_base) {
    (void)readl(gicd_base + GICD_CTLR);
    if (current_el() == 2)
        enable_sre_el2();

    gicv3_dist_init(gicd_base);
    gicv3_rd_init(gicr_base);
    gicv3_cpu_init(gicc_base);
}

/* Per-CPU GIC init (redistributor + CPU interface).
 * Must be called after MMU is enabled. Uses virtual addresses from
 * hyper_config()->host_gic.gicr_virt. */
void gicv3_pcpu_init(int pcpu_id)
{
    enable_sre_el2();

    uintptr_t gicr_base = hyper_config()->host_gic.gicr_virt +
                          (uintptr_t)pcpu_id * hyper_config()->host_gic.gicr_stride;

    /* Verify redistributor affinity via GICR_TYPER. */
    uint64_t typer = gicr_read_typer(gicr_base);
    uint64_t rdist_aff = (typer >> 32) & 0xff00ffffffULL;
    uint64_t my_aff = mrs(mpidr_el1) & 0xff00ffffffULL;
    if (rdist_aff != my_aff) {
        safe_printf("GICR%d typer aff 0x%lx != mpidr 0x%lx, skip\n",
                    pcpu_id, rdist_aff, my_aff);
        return;
    }

    wakeup_gic(gicr_base);
    gicv3_rd_init((void *)gicr_base);
    gicv3_cpu_init(NULL);
}

/* ---- Host physical IPI via SGI ---- */

static void ipi_send_sgi(uint64_t target_mpidr, uint8_t sgi_id)
{
    uint64_t aff1 = (target_mpidr >> 8) & 0xff;
    uint64_t aff2 = (target_mpidr >> 16) & 0xff;
    uint64_t aff3 = (target_mpidr >> 32) & 0xff;

    uint64_t sgir = GICV3_SGIR_VALUE(aff3, aff2, aff1, sgi_id,
                                     SGIR_IRM_TO_AFF, 1ULL);
    asm volatile("msr S3_0_C12_C11_5, %0" :: "r"(sgir) : "memory");
    isb();
}

static void ipi_broadcast_sgi(uint8_t sgi_id)
{
    /* IRM=1 -> all other PEs */
    uint64_t sgir = GICV3_SGIR_VALUE(0, 0, 0, sgi_id, 1, 0);
    asm volatile("msr S3_0_C12_C11_5, %0" :: "r"(sgir) : "memory");
    isb();
}

void ipi_send_cpu(int target_cpu, uint8_t ipi_vec)
{
    uint64_t mpidr = smp_cpu_to_mpidr(target_cpu);
    if (mpidr == (uint64_t)-1)
        return;
    ipi_send_sgi(mpidr, ipi_vec);
}

void ipi_broadcast_others(uint8_t ipi_vec)
{
    ipi_broadcast_sgi(ipi_vec);
}

void ipi_send_reschedule(int target_cpu)
{
    if (target_cpu != cpu_id())
        ipi_send_cpu(target_cpu, IPI_RESCHEDULE);
}

/* ---- Per-CPU call_func descriptor ---- */

struct ipi_call_desc {
    ipi_func_t fn;
    void      *arg;
};

/* One slot per pCPU. Written by sender before IPI, read by receiver in IPI handler. */
static struct ipi_call_desc g_ipi_call_desc[CONFIG_SMP_CPU_NUM];

void ipi_handle(uint8_t ipi_vec)
{
    switch (ipi_vec) {
    case IPI_RESCHEDULE:
        /* Handled by the caller - the IRQ handler will call sched_yield */
        break;
    case IPI_TLB_SHOOTDOWN:
        tlb_inv_guest_allis();
        break;
    case IPI_CALL_FUNC: {
        int me = cpu_id();
        ipi_func_t fn = g_ipi_call_desc[me].fn;
        void *arg = g_ipi_call_desc[me].arg;
        g_ipi_call_desc[me].fn = NULL;
        if (fn)
            fn(arg);
        break;
    }
    default:
        break;
    }
}

void ipi_pcpu_init(void)
{
    /* SGIs (0-15) are always enabled in GICv3, no extra setup needed. */
}

void ipi_tlb_shootdown(void)
{
    ipi_broadcast_others(IPI_TLB_SHOOTDOWN);
}

void ipi_call_func(int target_cpu, ipi_func_t fn, void *arg)
{
    if (target_cpu < 0 || target_cpu >= CONFIG_SMP_CPU_NUM)
        return;
    g_ipi_call_desc[target_cpu].fn  = fn;
    g_ipi_call_desc[target_cpu].arg = arg;
    ipi_send_cpu(target_cpu, IPI_CALL_FUNC);
}

