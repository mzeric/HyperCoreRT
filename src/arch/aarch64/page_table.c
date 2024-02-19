/*
    L0-L3 stage1/stage2 page tables
    L0-L2 hypervisor(stage1) page tables
*/
#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "cpu_inline_asm.h"
#include "execp.h"
#include <errno.h>



paddr_t vir_to_phy(vaddr_t v) {
    return (paddr_t)v;
}

uint64_t pte_offset(vaddr_t virt, uint8_t level) {
    uint64_t mask = (1UL << ARM_PT_LPAE_SHIFT) - 1;
    uint64_t val = (virt >> ARM_PT_LEVEL_SHIFT(level)) & mask;
    return val;
}

void build_p2m_table(lpae_t *cur_table, vaddr_t next_table, vaddr_t virt, uint8_t level, int attr) {
    int idx = pte_offset(virt, level);
    paddr_t addr = vir_to_phy(next_table);
    cur_table[idx] = make_p2m_table_entry(addr, attr);
}

void build_hyper_table(lpae_t *table_current_level, vaddr_t next_tbl, vaddr_t virt, uint8_t level, int attr) {
    int idx = pte_offset(virt, level);
    paddr_t addr = vir_to_phy(next_tbl);
    table_current_level[idx] = make_lpae_entry(addr >> 12, attr);
}

int __build_hyper_two_level_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t mem_size, int attr,
        lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2) {
    lpae_t *pte;

    build_hyper_table(table_L0, (vaddr_t)table_L1, virt_start, 0, attr);
    build_hyper_table(table_L1, (vaddr_t)table_L2, virt_start, 1, attr);

    pte = table_L2;

    paddr_t next_phy_addr = phys_start;
    paddr_t phys_end = phys_start + mem_size;
    /* fill L2 */
    if(next_phy_addr & (MB(2) - 1)){
        vmm_err("physical addr of level-2'next addr not aligned\n");
        return -1;
    }
    do {

        lpae_t e = make_lpae_entry(next_phy_addr >> 12, MT_NORMAL);
        e.pt.table = 0;
        e.pt.xn = 0;
        *pte = e;
    } while (next_phy_addr += MB(2), pte++, next_phy_addr < phys_end);

    return 0;
}


int __build_hyper_three_level_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t mem_size,
        int attr, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2,
        lpae_t* table_L3) {
    lpae_t* pte;
    vaddr_t addr;


    vmm_info("page_tables: %p, %p, %p\n", table_L0, table_L1, table_L2);
    build_hyper_table(table_L0, (vaddr_t)table_L1, virt_start, 0, attr);
    build_hyper_table(table_L1, (vaddr_t)table_L2, virt_start, 1, attr);

    addr = virt_start;
    pte = table_L2;

    paddr_t next_tbl = (uint64_t)table_L3;

    vaddr_t virt_end = virt_start + mem_size;
    paddr_t phys_end = phys_start + mem_size;

    /* fill L2 */

    do {

        lpae_t e = make_lpae_entry(next_tbl >> 12, MT_NORMAL);
        e.pt.xn = 0;
        *pte = e;
    } while (next_tbl += PAGE_SIZE, pte++, addr += (2 << 20), addr < virt_end);

    /* fill L3 */
    addr = (phys_start >> THIRD_SHIFT) << THIRD_SHIFT;
    lpae_t* entry = table_L3;

    do {
        *entry = make_lpae_entry(addr >> 12, MT_NORMAL);
        entry->pt.xn = 0;
    } while (entry++, addr += PAGE_SIZE, addr < phys_end);

    return 0;
}

int build_stage2_page_table(vaddr_t virt_start, paddr_t phys_start,
        uint64_t map_size, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2,
        lpae_t* table_L3, int attr) {
    build_p2m_table(table_L0, (vaddr_t)table_L1, virt_start, 0, attr);
    build_p2m_table(table_L1, (vaddr_t)table_L2, virt_start, 1, attr);


    int cnt = map_size / ARM_PT_LEVEL_SIZE(2);

    vaddr_t addr = virt_start;
    paddr_t val = vir_to_phy((vaddr_t)table_L3);
    for (int i = 0; i < cnt; ++i) {
        /* setup L1(second) entrys */
        build_p2m_table(table_L2, val, addr, 2, attr);
        val += PAGE_SIZE;
        addr += ARM_PT_LEVEL_SIZE(2);
    }

    /* fill L3: only map first map_size */
    cnt = map_size >> PAGE_SHIFT;

    paddr_t phy_start = virt_start;
    if (phy_start & 0xFFF) {
        vmm_err("physical addr of level-3'page addr not aligned\n");
        return -1;
    }

    for (int i = 0; i < cnt; ++i) {
        lpae_t p = make_p2m_table_entry(phy_start, attr);
        table_L3[i] = p;
        phy_start += PAGE_SIZE;
    }

    return 0;
}
