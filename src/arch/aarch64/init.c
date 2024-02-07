
#include "board_cfg_qemu_virt.h"
#include "config.h"
#include "system.h"
#include "vmmio.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>


typedef unsigned long irq_flags_t;
typedef unsigned long virtual_addr_t;
typedef unsigned long virtual_size_t;
typedef unsigned long physical_addr_t;
typedef unsigned long physical_size_t;

struct mmu_lpae_entry_ctrl {
	uint32_t ttbl_count;
	uint64_t *next_ttbl;
	virtual_addr_t ttbl_base;
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
    memset(start, 0, end - start);
}

void init_mmu() {

}

void setup_ttbl(void ) {


}

extern int _bss_start, _bss_end;

void init_hyper_low_level(struct board_info *info) {

    uint64_t el;

    asm("mrs	%0, CurrentEl": "=r" (el));
    if(el != SPSR_EL_M_EL2T) {
        vmm_err("current EL is't EL2T\n");
        panic();
    }

    /* for memset */
    uint32_t sctlr = get_sctlr();
    sctlr &= ~(CR_A);
    set_sctlr(sctlr);

    zero_bss(&_bss_start, &_bss_end);

    vmm_debug("el: 0x%x, current_el:0x%x\n", el, current_el());

}