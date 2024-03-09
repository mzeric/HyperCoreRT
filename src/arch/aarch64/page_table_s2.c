#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "cpu_inline_asm.h"
#include "excep.h"
#include <errno.h>
#include <string.h>

static const struct {
    unsigned int pabits;     /* physical address width */
    unsigned int root_order; /* page order for vttbr */
    unsigned int sl0;        /* sl0 for vtcr */
} pa_range_info[] = {

    /* T0SZ minimum and SL0 maximum from ARM DDI 0487H.a Table D5-6 */
    /*      PA size, t0sz(min), root-order, sl0(max) */

    [0] = {32, 0, 1},
    [1] = {36, 0, 1},
    [2] = {40, 1, 1},
    [3] = {42, 3, 1},
    [4] = {44, 0, 2},
    [5] = {48, 0, 2},
    [6] = {52, 4, 2},
    [7] = {0}, /* Invalid */
};

int pte_is_valid(lpae_t *pte) { return (pte->walk.valid && pte->pt.base); }

int s2_setup_info(struct stage2_mm_info *info, int pa_regs) {

    info->pa_size = pa_range_info[pa_regs].pabits;
    info->root_page_order = pa_range_info[pa_regs].root_order;
    info->lookup_level = 2 - pa_range_info[pa_regs].sl0;

    int order = info->root_page_order;
    if (order < 0 || order > 4) {
        vmm_fatal("invalide root_order: %d\n", order);
    }
    info->root_table = PAGE_VIR(alloc_pages(info->root_page_order));

    memset(info->root_table, 0, (1 << info->root_page_order) * PAGE_SIZE);

    vmm_info("pa_size:%d, root_table:%p, order:%d\n",
             info->pa_size,
             info->root_table,
             info->root_page_order);
}

static int s2_next_level(lpae_t **vtable, unsigned int level, unsigned int offset) {

    lpae_t *entry;
    int     ret;
    int     mfn;

    entry = *vtable + offset;

    if (level == 3 && pte_is_valid(entry)) {
        vmm_fatal("try remap existed s2 entry\n");
    }

    if (!pte_is_valid(entry)) {
        if (level != 3) {
            int fn = alloc_one_page();
            vmm_debug("alloc s2 page 0x%lx v:%p\n", fn, PAGE_VIR(fn));
            if (fn < 0) {
                vmm_fatal("out-of-memory for ptw map\n");
            }
            memset(PAGE_VIR(fn), 0, PAGE_SIZE);

            *entry = make_p2m_table_entry(PAGE_PHY(fn), p2m_ram_rw);
            // alloc a page for pte
        } else {
            // *entry = make_p2m_table_entry(, attr);
            vmm_fatal("should NOT be here\n");
        }
    }

    /* block desc */
    if (!entry->walk.table) {
        vmm_info("walk block desc\n");
        return 0;
    }

    /*
     table -> vaddr_t
     unmap *table first, when disable direct-mapping
    */
    *vtable = phy_to_vir(entry->walk.base << 12);
}

void write_pte(lpae_t *entry, lpae_t pte) {
    // vaddr_t *ventry = phy_to_vir(entry);
    *(lpae_t *)entry = pte;
}

int stage2_map_4k(lpae_t *root, int start_level, vaddr_t vaddr, paddr_t paddr) {
    lpae_t *table = NULL;
    int     end_level = 3;
    int     level;

    table = root;

    for (level = start_level; level < end_level; ++level) {
        // vmm_info("table:%p\n", table);
        s2_next_level(&table, level, pte_offset(vaddr, level));
    }

    // map last pte
    lpae_t *pte = table + pte_offset(vaddr, level);

    // vmm_debug("pte:%p = %p + %x\n", pte, table, pte_offset(vaddr, level));
    write_pte(pte, make_p2m_table_entry(paddr, p2m_ram_rw));
    if (end_level != 3)
        pte->walk.table = 0; //?

    return 0;
}

int stage2_map(struct stage2_mm_info *info, vaddr_t vaddr, paddr_t paddr, uint64_t map_size) {

    /* map size 4k aligned */
    lpae_t *root = info->root_table;
    int     start_level = info->lookup_level;

    map_size = (map_size + PAGE_SIZE - 1) & PAGE_MASK;

    for (u64 i = 0; i < map_size; i += PAGE_SIZE) {
        stage2_map_4k(root, start_level, vaddr, paddr);
        vaddr += PAGE_SIZE;
        paddr += PAGE_SIZE;
    }
}

int stage2_unmap(struct stage2_mm_info *info, vaddr_t vaddr, uint64_t map_size ) {

}

int build_static_stage2_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t map_size,
                            lpae_t *table_L0, lpae_t *table_L1, lpae_t *table_L2, lpae_t *table_L3,
                            int attr) {

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
        entry->p2m.xn = 1;
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
