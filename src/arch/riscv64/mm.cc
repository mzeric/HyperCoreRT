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

/* Host (HS-mode) page table root — Sv48, 4K-aligned */
__aligned(PAGE_SIZE) __section(".data.page_aligned") ptw_t page_table_root[512];

/* Guest (stage-2) page table root — Sv48x4 needs 16K alignment */
__aligned(PAGE_SIZE * 4) __section(".data.page_aligned") ptw_t page_stage2_table_root[512 * 4];

/* -------------------------------------------------------------------
 * HFENCE helpers
 * ------------------------------------------------------------------- */

static void hfence_gvma() {
    asm volatile(".insn r 0x73, 0x0, 0x31, x0, x0, x0" ::: "memory");
}

static void hfence_vvma() {
    asm volatile(".insn r 0x73, 0x0, 0x11, x0, x0, x0" ::: "memory");
}

void hfence() {
    hfence_vvma();
    hfence_gvma();
}

/* -------------------------------------------------------------------
 * Host direct mapping
 * ------------------------------------------------------------------- */

static int init_direct_mapping(ptw_t *root, int start_level, int attr, int without_mmu) {
    pg_map_huge(root, start_level, PAGE_VIRT_OFFSET, PAGE_PHYS_OFFSET,
                DIRECT_MAPPINT_PRIMARY_SIZE, attr, without_mmu);
    return 0;
}

/* -------------------------------------------------------------------
 * Guest memory setup — Stage-2 page tables
 *
 * Guest physical -> Host physical mappings for QEMU virt:
 *   0x90000000 (guest RAM)  -> 0x90000000 (host phys)  16MB, RWXU
 *   0x20000000 (guest UART) -> 0x10000000 (host UART)   4KB, RWU
 * ------------------------------------------------------------------- */

static void init_guest_memory() {
    /* Guest RAM: identity-mapped, guest sees its own physical addresses */
    pg_map(page_stage2_table_root, 0,
           0x90000000, 0x90000000, 0x1000000,
           PAGE_ATTR_EXEC | PAGE_ATTR_READ | PAGE_ATTR_WRITE | PAGE_ATTR_USER,
           0);

    /* Guest UART (0x20000000) -> Host UART (0x10000000) */
    pg_map(page_stage2_table_root, 0,
           0x20000000, 0x10000000, 0x1000,
           PAGE_ATTR_READ | PAGE_ATTR_WRITE | PAGE_ATTR_USER,
           0);

    /* Install stage-2 root into HGATP */
    u64 hgatp = (reinterpret_cast<uintptr_t>(page_stage2_table_root) >> 12)
                | (SATP_MODE_SV48 << 60);
    hfence();
    csrw(CSR_HGATP, hgatp);

    safe_printf("hgatp: %lx\n", csrr(CSR_HGATP));
}

/* -------------------------------------------------------------------
 * init_mm — full MMU initialization
 * ------------------------------------------------------------------- */

void init_mm() {
    init_page_allocator();

    /* Host: map hypervisor code + UART + direct mapping */
    pg_map(page_table_root, 0,
           0x80000000, 0x80000000, 0x2000000,
           PAGE_ATTR_READ | PAGE_ATTR_EXEC | PAGE_ATTR_WRITE, 1);

    pg_map(page_table_root, 0,
           0x10000000, 0x10000000, 0x1000,
           PAGE_ATTR_READ | PAGE_ATTR_WRITE, 1);

    init_direct_mapping(page_table_root, 0,
                        PAGE_ATTR_EXEC | PAGE_ATTR_READ | PAGE_ATTR_WRITE, 1);

    u64 satp = (reinterpret_cast<uintptr_t>(page_table_root) >> 12)
               | (SATP_MODE_SV48 << 60);
    enable_mmu(satp);

    init_kmalloc();

    /* Stage-2 for guest */
    init_guest_memory();

    safe_printf("mm init done\n");
}
