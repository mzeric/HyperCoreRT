
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


void init_mair(void) {
    msr(mair_el2, MAIRVAL);
}

struct mmu_lpae_entry_ctrl {
	uint32_t ttbl_count;
	uint64_t *next_ttbl;
	vaddr_t ttbl_base;
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

static inline void cpu_mmu_invalidate_range(vaddr_t start,
					    vaddr_t size)
{
	arm_inv_cache_range(start, start + size);
}

void panic(char *msg) {
    printf("panic.........\n");
    exit(1);

}

void zero_bss(void) {
    extern int _bss_start, _bss_end;
    size_t size = (size_t)&_bss_end - (size_t)&_bss_start;

    memset(&_bss_start, 0, size);
}

void init_stage1_mm(void ) {


}

extern void *__vmm_vectors;

void early_uart_init(void) {
    vmm_printf("UART/PL011 Enabled\n");
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
    uint64_t tcr_val = (TCR_RES1|TCR_SH0_IS|TCR_ORGN0_WBWA|TCR_IRGN0_WBWA|TCR_T0SZ(64-44));
    tcr_val |= (id<<16);

    /* such as: 0x8084_3510 */
    msr(tcr_el2, tcr_val);
    vmm_info("TCR_EL2:%x\n", tcr_val);
    msr_sync(SCTLR_EL2, SCTLR_EL2_SET);

    /*
    * Ensure that any exceptions encountered at EL2
    * are handled using the EL2 stack pointer, rather
    * than SP_EL0.
    */
    msr(spsel, 1);

}

void init_hyper_low_level(struct board_info *info) {

    uint64_t el;

    early_uart_init();

    asm("mrs	%0, CurrentEl": "=r" (el));
    if(current_el() != 2) {
        vmm_err("current EL is't EL2\n");
        panic("");
    }

    cpu_init();
#if 0
    /* for memset */
    uint32_t sctlr = get_sctlr();
    sctlr &= ~(CR_A);
    set_sctlr(sctlr);
#endif
    zero_bss();
    write_sysreg(&__vmm_vectors, vbar_el2);

    init_mm();
#if 0
    vmm_info("tcr: %x\n", TCR_EL2_VALUE);
    msr(tcr_el2, TCR_EL2_VALUE);
#endif
    vmm_debug("el: 0x%x, current_el:0x%x, cpu:%d\n", el, current_el(), aarch64_smp_id());

    vmm_info("=%x\n", mrs(VTTBR_EL2));
    switch_to_el1();
    /* test trap */
    // *(char*)(0xa00000000) = 0;

    vmm_info("current el: %d\n", current_el());
    vmm_info("here\n");

}