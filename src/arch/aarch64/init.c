
#include "board_cfg_qemu_virt.h"
#include "config.h"
#include "system.h"
#include "vmmio.h"
#include "cpu_inline_asm.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "processor.h"
#include "page.h"
#include "mm.h"
#include "excep.h"
#include "fdt_helper.h"
#include "sched.h"
#include "io.h"
#include "gicv3.h"
#include "vcpu.h"
#include "arch_barrier.h"
#include "sys_reg.h"

extern void timer_init();

struct mmu_lpae_entry_ctrl {
    uint32_t ttbl_count;
    uint64_t* next_ttbl;
    vaddr_t ttbl_base;
};

struct mm_region {
    uint64_t virt;
    uint64_t phys;
    uint64_t size;
    uint64_t attrs;
};
struct vmm_mmu {};

static inline void cpu_mmu_clean_invalidate(vaddr_t va) {
    asm volatile("dc civac, %0\t\n"
                 "dsb sy\t\n"
                 "isb\t\n"
                 :
                 : "r"((unsigned long)va));
}

void arm_inv_cache_range(const vaddr_t base, size_t size) {
    unsigned dcache_lsize = 0;
    static unsigned int cache_info = 0;
    const char* address;
    const char* end = (const char*)((uintptr_t)base + size);

    if (!cache_info) {
        /*  CTR_EL0 [3:0]   contains log2 of icache line size in words
         *  CTR_EL0 [19:16] contains log2 of dcache line size in words
         */
        asm volatile("mrs %0, ctr_el0" : "=r"(cache_info));
    }
    dcache_lsize = 4 << ((cache_info >> 16) & 0xF);

    address = (const char*)((uintptr_t)base & ~(uintptr_t)(dcache_lsize - 1));
    for (; address < (const char*)end; address += dcache_lsize) {
        asm volatile("dc ivac, %0" : : "r"(address) : "memory");
    }

    asm volatile("dsb sy; isb" : : : "memory");
}

void arm_flush_cache_range(const vaddr_t base, size_t size) {
    unsigned dcache_lsize = 0;
    static unsigned int cache_info = 0;
    const char* address;
    const char* end = (const char*)((uintptr_t)base + size);

    if (!cache_info) {
        /*  CTR_EL0 [3:0]   contains log2 of icache line size in words
         *  CTR_EL0 [19:16] contains log2 of dcache line size in words
         */
        asm volatile("mrs %0, ctr_el0" : "=r"(cache_info));
    }
    dcache_lsize = 4 << ((cache_info >> 16) & 0xF);

    address = (const char*)((uintptr_t)base & ~(uintptr_t)(dcache_lsize - 1));
    for (; address < (const char*)end; address += dcache_lsize) {
        asm volatile("dc cvac, %0" : : "r"(address) : "memory");
    }

    asm volatile("dsb sy\nisb" : : : "memory");
}

static inline void cpu_mmu_invalidate_range(vaddr_t start, vaddr_t size) {
    arm_inv_cache_range(start, start + size);
}

void zero_bss(void) {
    extern int _bss_start, _bss_end;
    size_t size = (size_t)&_bss_end - (size_t)&_bss_start;
    uint64_t *ptr = &_bss_start;
    size /= sizeof(*ptr);
    while(size--) {
        *ptr++ = 0;
    }
    // memset(&_bss_start, 0, size);
}

void init_stage1_mm(void) {}

extern void* __vmm_vectors;
static void *g_early_data_address;

void early_uart_init(void) {
    g_early_data_address = 0;
#ifdef CONFIG_BOARD_FVP_AEMVA
        g_early_data_address = 0x1c090000;
#elif  CONFIG_BOARD_QEMU_VIRT
        g_early_data_address = 0x09000000;
#endif

    // vmm_printf("UART/PL011 Enabled\n");
}

int id2pa_range(int id) {
    static const uint8_t pamax_map[] = {
            [0] = 32,
            [1] = 36,
            [2] = 40,
            [3] = 42,
            [4] = 44,
            [5] = 48,
            [6] = 52,
    };
    if (id < 0 || id > 6)
        return -1;
    return pamax_map[id];
}

int get_phys_id() {
    uint64_t id0 = mrs(ID_AA64MMFR0_EL1);

    return id0 & 0xF;
}

int get_phys_bits() {
    uint64_t id0 = mrs(ID_AA64MMFR0_EL1);
    return min(id2pa_range(id0 & 0xF), 48);
}


int load_dtb() {
    // char* fdt = (uint64_t*)0x40000000;

    char *fdt = (char *)ioremap_page(0x40001000, MT_NORMAL);

    u64 mem_addr;
    u64 mem_size;
    int root_node = -1;


    root_node = fdt_node_offset_by_compatible(fdt, -1, "hypervisor,platform");
    if (root_node < 0)
        vmm_fatal("not compatible with \"hypercorert\" found\n");
    int node =
        fdt_node_offset_by_prop_value(fdt, root_node, "device_type", "memory", sizeof("memory"));
    if (node < 0)
        vmm_fatal("no memory region found\n");
    vmm_debug("fdt %d\n", node);

    const char *name = fdt_get_name(fdt, node, NULL);
    if (fdt_get_reg_info(fdt, node, &mem_addr, &mem_size) < 0)
        vmm_fatal("memory fdt parse failed\n");
    vmm_info("\"%s\" -> <%p, 0x%lx>\n", name, mem_addr, mem_size);

    iounmap_page(fdt);
    return 0;
}

int cpu_init(void) {
    /* do NOT use printf here */

    uint64_t id0 = mrs(ID_AA64MMFR0_EL1); /* refs: arm:D7-2336 */
    uint64_t id1 = mrs(ID_AA64MMFR1_EL1);

    // vmm_info("ID_AA64MMFR1_EL1: 0x%p, 0x%p\n", id0, id1);
    /*
    bits[3:0] Physical Address range supported. Defined values are::
        0000 32 bits, 4GB.
        0001 36 bits, 64GB.
        0010 40 bits, 1TB.
        0011 42 bits, 4TB.
        0100 44 bits, 16TB.
        0101 48 bits, 256TB.
        0110 52 bits, 4PB.

        All other values are reserved.
    */
    // id = id > 5 ? 5 : id;

    int phys_bits = get_phys_bits();
    // vmm_debug("support %d(%d) physical address\n",  get_phys_bits(), id2pa_range(id0 & 0xF),
    //         get_phys_id());
    uint64_t tcr_val = (TCR_RES1 | TCR_SH0_IS | TCR_ORGN0_WBWA |
                        TCR_IRGN0_WBWA | TCR_T0SZ(64 - phys_bits));
    tcr_val |= (2 << 16); // SL0 = 2 => lookup level is 0

    if (phys_bits < 40) {
        // vmm_err("phys bits:%d unsupported\n", phys_bits);
        return -1;
    }

    /* such as: 0x8084_3510 */
    msr(tcr_el2, tcr_val);
    // vmm_info("TCR_EL2:%x\n", tcr_val);

    /* clear SCTLR.A */
    // msr_sync(SCTLR_EL2, SCTLR_EL2_SET);

    /*
     * Ensure that any exceptions encountered at EL2
     * are handled using the EL2 stack pointer, rather
     * than SP_EL0.
     */
    msr(spsel, 1);

    return 0;
}

void switch_to_el1();
void hyper_init_entry(void){


    safe_printf("init, current_el:%d\n", current_el());
    *(volatile int*)0x09000000 = 'S';
    // switch_to_el1();
    while (1) {
        wfi();

        safe_printf("init wakeup at el%d\n", current_el());
    }
}

void hyper_guard(void) {
    safe_printf("guard\n");
    while (1) {
        wfi();
        safe_printf("guard wakeup\n");
    }
}
void hyper_idle(void) {
    safe_printf("idle\n");
    while (1) {
        wfi();
        safe_printf("idle wakeup at el:%d\n", current_el());
    }
}

#define EL2_SET  (SCTLR_EL2_RES1 | SCTLR_EL2_EE_LE |\
			SCTLR_EL2_WXN_DIS | SCTLR_EL2_ICACHE_DIS |\
			SCTLR_EL2_SA_DIS | SCTLR_EL2_DCACHE_DIS |\
			SCTLR_EL2_ALIGN_DIS | SCTLR_EL2_MMU_DIS)

#if defined(CONFIG_BOARD_QEMU_VIRT)

#define GIC_GICD_BASE 0x08000000
#define GIC_GICC_BASE 0x08010000
#define GIC_GICR_BASE 0x080A0000

#elif defined(CONFIG_BOARD_FVP_AEMVA)

#define GIC_GICD_BASE 0x2f000000
#define GIC_GICC_BASE 0x2c000000
#define GIC_GICR_BASE 0x2f100000

#endif
/* GIC-600 specific register offsets */
#define GICR_PWRR			0x24U

/* GICR_PWRR fields */
#define PWRR_RDPD_SHIFT			0
#define PWRR_RDAG_SHIFT			1
#define PWRR_RDGPD_SHIFT		2
#define PWRR_RDGPO_SHIFT		3

#define PWRR_RDPD			(1U << PWRR_RDPD_SHIFT)
#define PWRR_RDAG			(1U << PWRR_RDAG_SHIFT)
#define PWRR_RDGPD			(1U << PWRR_RDGPD_SHIFT)
#define PWRR_RDGPO			(1U << PWRR_RDGPO_SHIFT)

/*
 * Values to write to GICR_PWRR register to power redistributor
 * for operating through the core (GICR_PWRR.RDAG = 0)
 */
#define PWRR_ON				(0U << PWRR_RDPD_SHIFT)
#define PWRR_OFF			(1U << PWRR_RDPD_SHIFT)

void early_delay(volatile int cnt) {
    volatile int i = 0;
    while (cnt--)
        i++;
}

void wakeup_gic(uintptr_t gicr_base) {
    u32 count = 100;


    writel(gicr_base + GICR_WAKER, ~(1u << 1));

    while(readl(gicr_base + GICR_WAKER) & GICR_WAKER_ChildrenAsleep) {
        count --;
        if(!count){
            safe_printf("wakeup timeout\n");
            return;
        }

        early_delay(1000);
    }
}
#include "gic_common.h"
#include "gic_common_private.h"
static inline void gicd_wait_for_pending_write(uintptr_t gicd_base)
{
	while ((gicd_read_ctlr(gicd_base) & GICD_CTLR_RWP_BIT) != 0U) {
	}
}
static inline void gicd_clr_ctlr(uintptr_t base,
				 unsigned int bitmap,
				 unsigned int rwp)
{
	gicd_write_ctlr(base, gicd_read_ctlr(base) & ~bitmap);
	if (rwp != 0U) {
		gicd_wait_for_pending_write(base);
	}
}

static inline void gicd_set_ctlr(uintptr_t base,
				 unsigned int bitmap,
				 unsigned int rwp)
{
	gicd_write_ctlr(base, gicd_read_ctlr(base) | bitmap);
	if (rwp != 0U) {
		gicd_wait_for_pending_write(base);
	}
}

int init_el3() {
    uintptr_t gicd_base = GIC_GICD_BASE;
    uintptr_t gicr_base = GIC_GICR_BASE;
    safe_printf("init_el3\n");
    wakeup_gic(gicr_base);
    msr(ICC_SRE_EL3, mrs(ICC_SRE_EL3) | 0xf);

#ifdef CONFIG_BOARD_FVP_AEMVA
    /* enable cntcr */
    writel(0x2a430000 + 0, 1);
    /* config 100M base frq */
    msr(cntfrq_el0, 0x5f5e100);
#endif
    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_IGROUPR0, ~0u);
    writel(gicr_base + GICD_RDIST_SGI_BASE + GICR_IGROUPR0 + 4, ~0u);

}

void switch_to_el2(void *stack, void *entry) {
    asm volatile("msr cptr_el3, xzr");

    msr(cptr_el2, CPTR_EL2_RES1);
    asm volatile("msr cntvoff_el2, xzr");

    init_el3();
    // msr(ICC_SRE_EL1, mrs(ICC_SRE_EL1) | 0xf);
    // msr(ICC_SRE_EL2, 1);
    // msr(ICC_SRE_EL2, mrs(ICC_SRE_EL2));


    msr(sctlr_el2, EL2_SET);
    msr(hcr_el2, 0);

    /* power on GICv3 for GIC600
    https://git.stikonas.eu/andrius/arm-trusted-firmware/commit/7a7fbb122ee3f66be81f34d58895939ef411e3f6
    */

    void *gicr_base = GIC_GICR_BASE;

    do {
        writel(gicr_base + GICR_PWRR, PWRR_ON);


    } while ((readl(gicr_base + GICR_PWRR) & PWRR_RDPD) != PWRR_ON);

    // hcr_val &= ~1;//disable vmmu;

    extern void *_guest_stack_end;
    extern void *_hvc_stack_end;

    // msr(sp_el2, &_guest_stack_end);
    uint64_t tmp;
    asm volatile("mov %0, sp\n\t"
                 " msr sp_el2, %0\n\t"
                 : "=r"(tmp)
                 : "r"(tmp)
                 : "memory");
    msr(spsr_el2, 0);

    uint64_t scr = mrs(scr_el3);
    tmp = (SCR_EL3_RW_AARCH64 | SCR_EL3_HCE_EN |\
			SCR_EL3_RES1 | SCR_EL3_NS_EN);
    scr |= 1;
    scr |= (1u << 10);
    scr &= ~(1ul << 3);

    msr(scr_el3, tmp);

    safe_printf("scr_el3:%x\n", tmp);


    tmp = (SPSR_EL_DEBUG_MASK | SPSR_EL_SERR_MASK |\
			SPSR_EL_IRQ_MASK | SPSR_EL_FIQ_MASK |\
			SPSR_EL_M_AARCH64 | SPSR_EL_M_EL2H);
    msr(spsr_el3, tmp);
    msr(elr_el3, entry);
    asm volatile("eret\t\n":::"memory");
    while(1);
}

int el2_init(void) {
    safe_printf("current EL is't EL2\n");
    while (1)
        ;
}
void _reset(void);
int __init_hyper_low_level(void *args);
void __armv8_switch_to_el2(void *entry, uint64_t flags);
/*******************************************************************************
 * MPIDR macros
 ******************************************************************************/
#define MPIDR_MT_MASK		(ULL(1) << 24)
#define MPIDR_CPU_MASK		MPIDR_AFFLVL_MASK
#define MPIDR_CLUSTER_MASK	(MPIDR_AFFLVL_MASK << MPIDR_AFFINITY_BITS)
#define MPIDR_AFFINITY_BITS	U(8)
#define MPIDR_AFFLVL_MASK	ULL(0xff)
#define MPIDR_AFF0_SHIFT	U(0)
#define MPIDR_AFF1_SHIFT	U(8)
#define MPIDR_AFF2_SHIFT	U(16)
#define MPIDR_AFF3_SHIFT	U(32)
#define MPIDR_AFF_SHIFT(_n)	MPIDR_AFF##_n##_SHIFT
#define MPIDR_AFFINITY_MASK	ULL(0xff00ffffff)
#define MPIDR_AFFLVL_SHIFT	U(3)
#define MPIDR_AFFLVL0		ULL(0x0)
#define MPIDR_AFFLVL1		ULL(0x1)
#define MPIDR_AFFLVL2		ULL(0x2)
#define MPIDR_AFFLVL3		ULL(0x3)
#define MPIDR_AFFLVL(_n)	MPIDR_AFFLVL##_n
#define MPIDR_AFFLVL0_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF0_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL1_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL2_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF2_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL3_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF3_SHIFT) & MPIDR_AFFLVL_MASK)

int init_hyper_low_level(void *args) {
    early_uart_init();

#if 0
    u64 core_id = smp_id();
    if (core_id != 0) {
        arch_spin_lock(&g_smp_lock);
        safe_printf("new core: \n");
        // safe_printf("\n");
        arch_spin_unlock(&g_smp_lock);
        while(1);

    }
    if (core_id == 0) {
        arch_spin_lock(&g_smp_lock);
        // safe_printf("core 0 up\n");
        // safe_printf("smp\n");
        arch_spin_unlock(&g_smp_lock);
        // while(1);
    }
#endif
    // while(1);
    safe_printf("current EL:%d\n", current_el());
    if (current_el() == 3) {
        /* swith to EL2 */
        // switch_to_el2(el2_init);
        isb();
        arch_mb();

        safe_printf("switch to EL2...\n");
        isb();
        arch_mb();
        switch_to_el2(NULL, __init_hyper_low_level);
        // __armv8_switch_to_el2(__init_hyper_low_level, 1);
        // asm volatile("msr ");
    }

    if (current_el() != 2) {

        safe_printf("current EL:%d is't EL2\n", current_el());
        return -1;
    }

    return __init_hyper_low_level(args);
}


int __init_hyper_low_level(void *args) {

    uint64_t el;
    safe_printf("init low level\n");
    write_sysreg(&__vmm_vectors, vbar_el2);

    // zero_bss();
    safe_printf("z-bss done\n");
    cpu_init();
    safe_printf("cpu-init done\n");

    init_mm();
    safe_printf("mmu-init done\n");

#if 0
    /* for memset */
    uint32_t sctlr = get_sctlr();
    sctlr &= ~(CR_A);
    set_sctlr(sctlr);
#endif
    write_sysreg(&__vmm_vectors, vbar_el2);

#if 0
    vmm_info("tcr: %x\n", TCR_EL2_VALUE);
    msr(tcr_el2, TCR_EL2_VALUE);
#endif
    vmm_debug("el: 0x%x, current_el:0x%x, cpu:%d\n", el, current_el(),
            smp_id());

    vmm_info("=%x\n", mrs(VTTBR_EL2));

    // load_dtb();

    /* test trap */
    // *(char*)(0xa00000000) = 0;



    void *gicd_base = (void*)ioremap_page(GIC_GICD_BASE, MT_DEVICE_nGnRnE);
    void *gicc_base = (void*)ioremap_page(GIC_GICC_BASE, MT_NORMAL);
    void *gicr_base = (void*)ioremap(GIC_GICR_BASE, 0x200000, MT_DEVICE_nGnRnE);

    vmm_info("here\n");

    vmm_info("GICv - %x\n", readl(gicd_base + GICD_CTLR));

    if (readl(gicd_base + 0x4) == 0x68)
        init_gicv2(gicd_base, gicc_base);
    else
        init_gicv3(gicd_base, gicc_base, gicr_base);

    iounmap_page(gicd_base);
    iounmap_page(gicc_base);
    iounmap(gicr_base, 0x200000);

    init_sched();

    uint32_t v;
    asm volatile("mov %0, sp":"=r"(v)::"memory");

    safe_printf("here stack:%p\n",v);
    timer_init();
    create_task("init", hyper_init_entry, 10);

    create_task("idle", hyper_idle, 10);
    while(1);
    // create_task("guard", hyper_guard, 10);


    vmm_info("HyperCoreRT boot finished\n");
    // switch_to_el1();

    test_vcpu();

    /* switch to el2 go down here, has no way out */
    while (1)
        ;
    return 0;
}
