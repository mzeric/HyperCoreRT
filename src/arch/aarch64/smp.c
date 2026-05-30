#include "smp.h"
#include "safe_printf.h"
#include "inline_asm.h"
#include "hyper_config.h"
#include "vmio.h"
#include "log.h"
#include "mm.h"
#include "mmu.h"
#include "timer.h"
#include "excep.h"
#include "src/drivers/gic/gicv3.h"
#include "ipi.h"

#include <libfdt.h>
#include <stdint.h>
#include <string.h>

/* ---- PSCI constants ---- */
#define PSCI_FNID_VERSION  0x84000000UL
#define PSCI_FNID_CPU_ON   0xC4000003UL
#define PSCI_RET_SUCCESS   0

/* ---- Global state ---- */
static struct host_cpu_desc host_cpus[SMP_MAX_CPUS];
static int host_cpu_count;

/* ---- PSCI SMC helpers ---- */
static uint32_t psci_version(void)
{
    register uint64_t x0 __asm__("x0") = PSCI_FNID_VERSION;
    __asm__ volatile("smc #0"
                     : "+r"(x0)
                     :
                     : "memory");
    return (uint32_t)x0;
}

static int psci_cpu_on(uint64_t target_mpidr, uint64_t entry, uint64_t context_id)
{
    register uint64_t x0 __asm__("x0") = PSCI_FNID_CPU_ON;
    register uint64_t x1 __asm__("x1") = target_mpidr;
    register uint64_t x2 __asm__("x2") = entry;
    register uint64_t x3 __asm__("x3") = context_id;
    __asm__ volatile("smc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3)
                     : "memory");
    return (int)x0;
}

/* ---- Early MPIDR -> linear CPU ID ---- */
int smp_mpidr_to_cpu(uint64_t mpidr)
{
    for (int i = 0; i < host_cpu_count; i++) {
        if (host_cpus[i].mpidr == mpidr)
            return i;
    }
    return -1;
}

int smp_current_cpu_id(void)
{
    return smp_mpidr_to_cpu(smp_id());
}

uint64_t smp_cpu_to_mpidr(int cpu)
{
    if (cpu < 0 || cpu >= host_cpu_count)
        return (uint64_t)-1;
    return host_cpus[cpu].mpidr;
}

int smp_cpu_count(void)
{
    return host_cpu_count;
}

/* ---- DTB /cpus parser ---- */
static int parse_host_cpus(void *fdt)
{
    int cpus_node = fdt_path_offset(fdt, "/cpus");
    if (cpus_node < 0)
        return -1;

    host_cpu_count = 0;
    uint64_t boot_mpidr = smp_id();

    for (int node = fdt_first_subnode(fdt, cpus_node);
         node >= 0;
         node = fdt_next_subnode(fdt, node)) {

        if (host_cpu_count >= SMP_MAX_CPUS)
            break;

        /* Read reg = MPIDR affinity */
        int len;
        const fdt32_t *reg = fdt_getprop(fdt, node, "reg", &len);
        if (!reg)
            continue;

        int parent = fdt_parent_offset(fdt, node);
        int na = 2; /* default #address-cells */
        int na_len;
        const fdt32_t *na_prop = fdt_getprop(fdt, parent, "#address-cells", &na_len);
        if (na_prop && na_len >= 4)
            na = fdt32_to_cpu(na_prop[0]);

        if (len < na * 4)
            continue;

        uint64_t mpidr = 0;
        for (int i = 0; i < na; i++)
            mpidr = (mpidr << 32) | fdt32_to_cpu(reg[i]);
        mpidr &= 0xff00ffffffULL;

        /* Skip non-PSCI CPUs */
        const char *method = fdt_getprop(fdt, node, "enable-method", &len);
        if (!method || strncmp(method, "psci", 4) != 0)
            continue;

        int cid = host_cpu_count;
        host_cpus[cid].mpidr  = mpidr;
        host_cpus[cid].cpu_id = cid;
        host_cpus[cid].state  = CPU_OFFLINE;
        host_cpu_count++;

        if (mpidr != boot_mpidr) {
            hyper_info("host cpu%d: mpidr=0x%lx, psci", cid, mpidr);
        }
    }

    return host_cpu_count;
}

/* ---- Secondary entry (C) ---- */

extern void gic_vcpu_init_pcpu(void);

void secondary_start(void)
{
    int cpu = smp_current_cpu_id();
    uint64_t mpidr = smp_id();

    if (cpu < 0) {
        /* Unknown CPU — can't safely init, just park */
        while (1) { wfi(); }
    }

    /* Phase 1: enable MMU (load primary's config from g_mmu_boot) */
    msr(tcr_el2, g_mmu_boot.tcr_el2);
    msr_sync(vtcr_el2, g_mmu_boot.vtcr_el2);
    msr_sync(VTTBR_EL2, g_mmu_boot.vttbr_el2);
    msr_sync(TTBR0_EL2, boot_pgtable);
    asm volatile("isb");
    mmu_enable();

    /* Phase 2: per-pCPU GIC init using virtual addresses (after MMU) */
    gicv3_pcpu_init(cpu);
    gic_vcpu_init_pcpu();
    ipi_pcpu_init();

    /* Phase 3: configure HCR_EL2 for stage-2 + trap routing */
    {
        uint64_t hcr = get_default_hcr_flags();
        hcr |= (1UL << 31);           /* RW: EL1 is AArch64 */
        hcr |= (1UL << 42) | (1UL << 43) | (1UL << 45); /* TSC, TAC, TTLB */
        asm volatile("msr hcr_el2, %0; isb" :: "r"(hcr));
    }

    /* Phase 4: enable EL2 physical timer for scheduling tick */
    enable_timer_irq();
    hyp_timer_rearm();

    /* Mark online */
    host_cpus[cpu].state = CPU_ONLINE_SCHED;
    __asm__ volatile("dmb ish; sev" ::: "memory");

    hyper_info("pcpu%d online, mpidr=0x%lx, cpu_id=%d, entering scheduler", cpu, mpidr, cpu_id());

    while (1) {
        wfi();
    }
}

/* ---- Primary: boot all secondaries ---- */

/* Provided by head.S */
extern char secondary_entry[];

void smp_boot_secondaries(void *fdt)
{
    parse_host_cpus(fdt);

    /* Probe PSCI first */
    uint32_t ver = psci_version();
    hyper_info("psci version: 0x%x (major=%u minor=%u)",
               ver, ver >> 16, ver & 0xffff);

    if (host_cpu_count <= 1) {
        hyper_info("no secondary cpus to boot");
        return;
    }

    /* secondary_entry is linked at its physical address (identity mapping),
     * so its virtual address == physical address in the current setup.
     * PSCI CPU_ON expects a physical address.
     */
    uint64_t entry_addr = (uint64_t)secondary_entry;

    for (int i = 0; i < host_cpu_count; i++) {
        if (host_cpus[i].mpidr == smp_id())
            continue; /* skip boot CPU */

        hyper_info("psci cpu_on mpidr=0x%lx entry=0x%lx",
                   host_cpus[i].mpidr, entry_addr);

        int ret = psci_cpu_on(host_cpus[i].mpidr, entry_addr, 0);
        if (ret != PSCI_RET_SUCCESS) {
            hyper_warn("psci cpu_on failed: %d (0x%lx)", ret, (uint64_t)ret);
            continue;
        }

        /* Wait for secondary to come online and enter scheduler.
         * yield is safe for QEMU TCG; wfe may not wake on sev. */
        int timeout = 100000;
        while (host_cpus[i].state < CPU_ONLINE_SCHED && timeout-- > 0)
            __asm__ volatile("yield" ::: "memory");

        if (timeout <= 0)
            hyper_warn("pcpu%d boot timeout", i);
    }

    hyper_info("all secondary cpus in scheduler mode");
}
