
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
// #include <sys/types.h>

/* MAIR_EL2 encodings */
#define MAIR(val, idx)                  ((val) << ((idx) * 8))
/* Attribute Indices */
#define AINDEX_DEVICE_nGnRnE            0
#define AINDEX_DEVICE_nGnRE         1
#define AINDEX_DEVICE_nGRE          2
#define AINDEX_DEVICE_GRE           3
#define AINDEX_NORMAL_WT            4
#define AINDEX_NORMAL_WB            5
#define AINDEX_NORMAL_NC            6
#define MAIR_EL2_VALUE                                                         \
    (MAIR(0x00, AINDEX_DEVICE_nGnRnE) | MAIR(0x04, AINDEX_DEVICE_nGnRE) |      \
            MAIR(0x08, AINDEX_DEVICE_nGRE) | MAIR(0x0c, AINDEX_DEVICE_GRE) |   \
            MAIR(0xbb, AINDEX_NORMAL_WT) | MAIR(0xff, AINDEX_NORMAL_WB) |      \
            MAIR(0x44, AINDEX_NORMAL_NC))

#define TCR_EL2_VALUE                                                          \
    (TCR_T0SZ_VAL(39) | TCR_PS_40BITS | (0x0 << TCR_TG0_SHIFT) |               \
            (0x3 << TCR_SH0_SHIFT) | (0x1 << TCR_ORGN0_SHIFT) |                \
            (0x1 << TCR_IRGN0_SHIFT))

typedef unsigned long irq_flags_t;
typedef unsigned long virtual_addr_t;
typedef unsigned long virtual_size_t;
typedef unsigned long physical_addr_t;
typedef unsigned long physical_size_t;


void init_mair(void) {
    msr(mair_el2, MAIRVAL);
}

struct mmu_lpae_entry_ctrl {
	uint32_t ttbl_count;
	uint64_t *next_ttbl;
	virtual_addr_t ttbl_base;
};

struct mm_region {
    uint64_t virt;
    uint64_t phys;
    uint64_t size;
    uint64_t attrs;
};
struct vmm_mmu {

};

static inline void cpu_mmu_clean_invalidate(void *va)
{
	asm volatile("dc civac, %0\t\n"
		     "dsb sy\t\n"
		     "isb\t\n"
		     : : "r" ((unsigned long)va));
}

void arm_inv_cache_range(const void *base, size_t size)
{
    unsigned            dcache_lsize = 0;
    static unsigned int cache_info   = 0;
    const char *        address;
    const char *        end = (const char *)((uintptr_t)base + size);

    if (!cache_info) {
        /*  CTR_EL0 [3:0]   contains log2 of icache line size in words
         *  CTR_EL0 [19:16] contains log2 of dcache line size in words
         */
        asm volatile("mrs %0, ctr_el0" : "=r"(cache_info));
    }
    dcache_lsize = 4 << ((cache_info >> 16) & 0xF);

    address = (const char *)((uintptr_t)base & ~(uintptr_t)(dcache_lsize - 1));
    for (; address < (const char *)end; address += dcache_lsize) {
        asm volatile("dc ivac, %0" : : "r"(address) : "memory");
    }

    asm volatile("dsb sy; isb" : : : "memory");
}

void arm_flush_cache_range(const void *base, size_t size)
{
    unsigned            dcache_lsize = 0;
    static unsigned int cache_info   = 0;
    const char *        address;
    const char *        end = (const char *)((uintptr_t)base + size);

    if (!cache_info) {
        /*  CTR_EL0 [3:0]   contains log2 of icache line size in words
         *  CTR_EL0 [19:16] contains log2 of dcache line size in words
         */
        asm volatile("mrs %0, ctr_el0" : "=r"(cache_info));
    }
    dcache_lsize = 4 << ((cache_info >> 16) & 0xF);

    address = (const char *)((uintptr_t)base & ~(uintptr_t)(dcache_lsize - 1));
    for (; address < (const char *)end; address += dcache_lsize) {
        asm volatile("dc cvac, %0" : : "r"(address) : "memory");
    }

    asm volatile("dsb sy\nisb" : : : "memory");
}

static inline void cpu_mmu_invalidate_range(virtual_addr_t start,
					    virtual_addr_t size)
{
	arm_inv_cache_range(start, start + size);
}

void panic() {
    printf("panic.........\n");
    exit(1);

}

void zero_bss(void *start, void *end) {
    vmm_debug("zero bss from %p -> %p\n", start, end);
    memset(start, 0, end - start);
}


void init_stage1_mm(void ) {



}

extern int _bss_start, _bss_end;
extern void *__vmm_vectors;

void early_uart_init(void) {
    vmm_printf("UART/PL011 Enabled");
}

void cpu_init(void) {
    init_mair();

    uint64_t id = mrs(ID_AA64MMFR0_EL1) & 0xF; /* refs: arm:D7-2336 */
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
    vmm_debug("support %d physical address\n", id == 4 ? 44 : id == 5 ? 48 : -1);
    uint64_t tcr_val = (TCR_RES1|TCR_SH0_IS|TCR_ORGN0_WBWA|TCR_IRGN0_WBWA|TCR_T0SZ(64-48));
    tcr_val |= (id<<16);

    /* such as: 0x8084_3510 */
    msr(tcr_el2, tcr_val);
    msr_sync(SCTLR_EL2, SCTLR_EL2_SET);
    /*
    * Ensure that any exceptions encountered at EL2
    * are handled using the EL2 stack pointer, rather
    * than SP_EL0.
    */
    msr(spsel, 1);

}

static paddr_t p_start = 0x40000000;
static paddr_t p_end = 0x50000000;

void init_hyper_low_level(struct board_info *info) {

    uint64_t el;

    early_uart_init();

    asm("mrs	%0, CurrentEl": "=r" (el));
    if(current_el() != 2) {
        vmm_err("current EL is't EL2\n");
        panic();
    }

    cpu_init();
#if 0
    /* for memset */
    uint32_t sctlr = get_sctlr();
    sctlr &= ~(CR_A);
    set_sctlr(sctlr);
#endif
    zero_bss(&_bss_start, &_bss_end);
    write_sysreg(&__vmm_vectors, vbar_el2);

    init_mm();
#if 0
    vmm_info("tcr: %x\n", TCR_EL2_VALUE);
    msr(tcr_el2, TCR_EL2_VALUE);
#endif
    vmm_debug("el: 0x%x, current_el:0x%x, cpu:%d\n", el, current_el(), aarch64_smp_id());

    vmm_info("=%x\n", mrs(VTTBR_EL2));

    *(char*)(0xa00000000) = 0;
    3/0;

    vmm_info("here\n");

}