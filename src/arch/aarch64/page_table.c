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



uint64_t pte_offset(vaddr_t virt, uint8_t level) {
    uint64_t mask = (1UL << ARM_PT_LPAE_SHIFT) - 1;
    uint64_t val = (virt >> ARM_PT_LEVEL_SHIFT(level)) & mask;
    return val;
}

void build_s2_table(lpae_t *cur_table, vaddr_t next_table, vaddr_t virt, uint8_t level, int attr) {
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

    pte = &table_L2[pte_offset(virt_start, 2)];

    paddr_t phys_end = phys_start + mem_size;
    paddr_t next_phy_addr = phys_start & (~(ARM_PT_LEVEL_SIZE(2) - 1));

    if(next_phy_addr & (MB(2) - 1)){
        vmm_fatal("physical addr of level-2'next addr not aligned\n");
    }

    /* fill L2 */
    do {

        lpae_t e = make_lpae_entry(next_phy_addr >> 12, MT_NORMAL);
        e.pt.table = 0;
        e.pt.xn = 0;
        *pte = e;
    } while (next_phy_addr += MB(2), pte++, next_phy_addr < phys_end);

    return 0;
}

void print_addr_idx(vaddr_t addr) {

    vmm_info("offset[%p]= <%p, %p, %p, %p>\n", addr, pte_offset(addr, 0), pte_offset(addr, 1),
             pte_offset(addr, 2), pte_offset(addr, 3));
}

int __build_hyper_three_level_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t mem_size,
        int attr, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2,
        lpae_t* table_L3) {

    lpae_t* entry = NULL;
    vaddr_t addr;

    vmm_debug("page_tables: %p, %p, %p, %p\n", table_L0, table_L1, table_L2, table_L3);
    vmm_debug("map from %p:%p size: %p\n", virt_start, phys_start, mem_size);
    build_hyper_table(table_L0, (vaddr_t)table_L1, virt_start, 0, attr);
    build_hyper_table(table_L1, (vaddr_t)table_L2, virt_start, 1, attr);


    paddr_t next_tbl = (uint64_t)table_L3;

    vaddr_t virt_end = virt_start + mem_size;
    paddr_t phys_end = phys_start + mem_size;

    /* fill L2 */
    addr = virt_start & (~(ARM_PT_LEVEL_SIZE(2) - 1));
    entry = &table_L2[pte_offset(virt_start, 2)];

    do {

        lpae_t e = make_lpae_entry(next_tbl >> 12, MT_NORMAL);
        e.pt.xn = 0;
        *entry = e;
    } while (next_tbl += PAGE_SIZE, entry++, addr += ARM_PT_LEVEL_SIZE(2), addr < virt_end);

    /* fill L3 */
    addr = (phys_start >> THIRD_SHIFT) << THIRD_SHIFT;
    entry = &table_L3[pte_offset(virt_start, 3)];

    do {
        *entry = make_lpae_entry(addr >> 12, MT_NORMAL);
        entry->pt.xn = 0;
    } while (entry++, addr += ARM_PT_LEVEL_SIZE(3), addr < phys_end);

    return 0;
}

int build_stage2_page_table(vaddr_t virt_start, paddr_t phys_start,
        uint64_t map_size, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2,
        lpae_t* table_L3, int attr) {

    lpae_t *entry = NULL;
    vaddr_t addr;

    vmm_debug("map size: %lx from %p:%p\n", map_size, virt_start, phys_start);
    build_s2_table(table_L0, (vaddr_t)table_L1, virt_start, 0, attr);
    build_s2_table(table_L1, (vaddr_t)table_L2, virt_start, 1, attr);


    addr = virt_start & (~(ARM_PT_LEVEL_SIZE(2) - 1));
    entry = &table_L2[pte_offset(virt_start, 2)];
    paddr_t next_tbl = vir_to_phy((vaddr_t)table_L3);

    do {

        *entry = make_p2m_table_entry(next_tbl, attr);
        next_tbl += PAGE_SIZE;
        addr += ARM_PT_LEVEL_SIZE(2);

    } while (entry++, addr < (virt_start + map_size));


    /* fill L3 */
    paddr_t phy_start = virt_start;
    paddr_t phy_end = phy_start + map_size;
    entry = &table_L3[pte_offset(virt_start, 3)];

    if (phy_start & 0xFFF) {
        vmm_err("physical addr of level-3'page addr not aligned\n");
        return -1;
    }

    do {

        *entry = make_p2m_table_entry(phy_start, attr);
        phy_start += ARM_PT_LEVEL_SIZE(3);

    } while (entry++, phy_start < phy_end);

    return 0;
}

paddr_t __walk_page_table(lpae_t *cur_tbl, vaddr_t addr, int level) {

    paddr_t next_tbl_phy;
    lpae_t* next_tbl_vir;


    if (!cur_tbl || level < 0 || level > 3)
        return 0;

    lpae_t next_pte = cur_tbl[pte_offset(addr, level)];
    next_tbl_phy = next_pte.pt.base << 12;

    if(next_pte.pt.valid == 0)
        return 0;


    vmm_info("ptw cur:%p -> %p\n", cur_tbl, next_tbl_phy);
    if (level == 3 || next_pte.pt.table == 0)
        return next_tbl_phy;

    next_tbl_vir = (lpae_t*)phy_to_vir(next_tbl_phy);

    return __walk_page_table(next_tbl_vir, addr, level + 1);
}

void* kmap(paddr_t paddr) {

    return NULL;
}