#include "htypes.h"
#include "mm.h"
#include "compiler.h"

#include "bitmap.h"
#include "page_alloc.h"
#include "safe_printf.h"
#include "inline_asm.h"
#include "riscv64_system.h"
#include "mmu.h"
#include "kmalloc.h"

#include "arch_page.h"
#include "sbi_helper.h"

/*
    align 4K L0 page-root
*/
__aligned(PAGE_SIZE) __section(".data.page_aligned") ptw_t page_table_root[512];
__aligned(PAGE_SIZE * 4) __section(".data.page_aligned") ptw_t page_stage2_table_root[512 * 4];


int init_direct_mapping(ptw_t *root, int start_level, int attr, int without_mmu ) {

    uint64_t map_size = DIRECT_MAPPINT_PRIMARY_SIZE;
    safe_printf("mapping %lx -> %lx\n", PAGE_VIRT_OFFSET, PAGE_PHYS_OFFSET);

    pg_map_huge(root,
                start_level,
                PAGE_VIRT_OFFSET,
                PAGE_PHYS_OFFSET,
                map_size,
                attr, without_mmu);

    return 0;
}
#include <stdio.h>

void vm_entry() {
    safe_printf("vs_mode?\n");
    *(int*)0x30000000 = 'G';
    // safe_printf("satp:%lx\n", csrr(0x180));
    // sbi_set_timer(1);
    while(1);
}
static inline void hfence_gvma() {
    asm volatile(
        ".insn r 0x73, 0x0, 0x31, x0, x0, x0\n\t"
        ::: "memory");
}

static inline void hfence_vvma() {
    asm volatile(
        ".insn r 0x73, 0x0, 0x11, x0, x0, x0\n\t"
        ::: "memory");
}

void hfence() {
	hfence_vvma();
	hfence_gvma();
}
void switch_to_vs() {
    u64 hgatp = csrr(CSR_HSTATUS);
    // hgatp &= ~(1ul<<21);
    csrw(CSR_HSTATUS, hgatp | (1 << 7));

    hfence();

    asm volatile("la t0, 1f\n\t"
                 "csrw sepc, t0\n\t"
                 "sret\n\t"
                 "1:\n\t");
}

void setup_stage2_pages() {
    u64 val;
    val = csrr(sstatus);
    val |= 1u << 8;
    csrw(sstatus, val);

    val = csrr(CSR_HSTATUS);
    val |= HSTATUS_SPV | HSTATUS_SPVP;
    csrw(CSR_HSTATUS, val);

    // write stage2 regs
    pg_map(page_stage2_table_root,
           0,
           0x90000000,
           0x90000000,
           0x1000000,
           PAGE_ATTR_EXEC | PAGE_ATTR_READ | PAGE_ATTR_WRITE | PAGE_ATTR_USER,
           0);
#if 0
    pg_map(page_stage2_table_root,
           0,
           0x10000000,
           0x10000000,
           0x1000,
           PAGE_ATTR_READ | PAGE_ATTR_WRITE | PAGE_ATTR_USER,
           0);
#endif
    pg_map(page_stage2_table_root,
           0,
           0x20000000,
           0x10000000,
           0x1000,
           PAGE_ATTR_READ | PAGE_ATTR_WRITE | PAGE_ATTR_USER,
           0);


    init_direct_mapping(page_stage2_table_root,
                        0,
                        PAGE_ATTR_EXEC | PAGE_ATTR_READ | PAGE_ATTR_WRITE | PAGE_ATTR_USER,
                        0);

    u64 satp_val;
    satp_val = ((uintptr_t)page_stage2_table_root >> 12);
    satp_val |= SATP_MODE_SV48 << 60;
    // __sfence_vma_all();
    hfence();
    csrw(CSR_HGATP, satp_val);
    // ptw_walk(page_stage2_table_root, 0x80083000, 1);

    safe_printf("test hgatp %lx / %lx\n", csrr(CSR_HGATP), page_stage2_table_root);

    // switch_to_vs();

    // vm_entry();

}

void init_mm() {

    init_page_allocator();


    int fn = alloc_pages(0);
    int fn2 = alloc_pages(0);
    safe_printf("fn:%lx, fn2:%lx\n", fn, fn2);

    paddr_t paddr = 0x80080000;
    vaddr_t vaddr = 0x80080000;
    u64     map_size = 0x2000000;

    pg_map(page_table_root,
           0,
           vaddr,
           paddr,
           map_size,
           PAGE_ATTR_READ | PAGE_ATTR_EXEC | PAGE_ATTR_WRITE, 1);

    pg_map(page_table_root, 0, 0x10000000, 0x10000000, 0x1000, PAGE_ATTR_READ | PAGE_ATTR_WRITE, 1);

    init_direct_mapping(page_table_root, 0, PAGE_ATTR_EXEC | PAGE_ATTR_READ | PAGE_ATTR_WRITE , 1);

    u64 satp_val;
    satp_val = ((uintptr_t)page_table_root >> 12);
    satp_val |= SATP_MODE_SV48 << 60;
    safe_printf("satp_val %lx\n", satp_val);


    enable_mmu(satp_val);

    init_kmalloc();
    // init_kmap();

    printf("hello\n");

    setup_stage2_pages();


    safe_printf("mmu init done: pfn:%lx, %lx\n", page_table_root, csrr(sptbr));

}