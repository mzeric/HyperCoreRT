#include "page.h"
#include "aarch64_hcr.h"
#include "vmio.h"
#include "mm.h"
#include "inline_asm.h"
#include "excep.h"
#include "spin_lock.h"
#include "mmu.h"
#include "ipi.h"
#include "smp.h"
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

void s2_pte_access(lpae_t *pte, int af) {
    pte->p2m.af = 0;
}

int s2_setup_info(struct stage2_mm_info *info, int pa_regs) {

    info->pa_size = pa_range_info[pa_regs].pabits;
    info->root_page_order = pa_range_info[pa_regs].root_order;
    info->lookup_level = 2 - pa_range_info[pa_regs].sl0;

    return 0;
}

int s2_alloc_root_pages(struct stage2_mm_info *info) {
    int order = info->root_page_order;
    if (order < 0 || order > 4) {
        hyper_fatal("invalide root_order: %d", order);
    }
    info->root_table = PAGE_VIR(alloc_pages(info->root_page_order));

    memset((void *)info->root_table, 0, (1 << info->root_page_order) * PAGE_SIZE);

    hyper_info("pa_size:%d, root_table:%lx, order:%d",
             info->pa_size,
             info->root_table,
             info->root_page_order);

    return 0;
}

static int s2_next_level_map(lpae_t **vtable, unsigned int level, unsigned int offset) {

    lpae_t *entry;

    entry = *vtable + offset;

    if (level == 3 && pte_is_valid(entry)) {
        hyper_fatal("try remap existed s2 entry");
    }

    if (!pte_is_valid(entry)) {
        if (level != 3) {
            int fn = alloc_one_page();
            // hyper_debug("alloc s2 page 0x%lx v:%p", fn, PAGE_VIR(fn));
            if (fn < 0) {
                hyper_fatal("out-of-memory for ptw map");
            }
            void *p = (void *)PAGE_VIR(fn);
            if (p)
                memset(p, 0, PAGE_SIZE);

            *entry = make_stage2_entry(PAGE_PHY(fn), MEM_NORMAL_RW, MEM_ACCESS_RWX);
            // alloc a page for pte
        } else {
            // *entry = make_stage2_entry(, attr);
            hyper_fatal("should NOT be here");
        }
    }

    /* block desc */
    if (!entry->walk.table) {
        hyper_info("walk block desc");
        return 0;
    }

    /*
     table -> vaddr_t
     unmap *table first, when disable direct-mapping
    */
    *vtable = (lpae_t *)phy_to_vir(entry->walk.base << 12);

    return 0;
}

void write_pte(lpae_t *entry, lpae_t pte) {
    // vaddr_t *ventry = phy_to_vir(entry);
    *(lpae_t *)entry = pte;
}

int stage2_map_4k(lpae_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, int attr, int acc) {
    lpae_t *table = NULL;
    int     end_level = 3;
    int     level;

    table = root;

    for (level = start_level; level < end_level; ++level) {
        // hyper_info("table:%p", table);
        s2_next_level_map(&table, level, pte_offset(vaddr, level));
    }

    // map last pte
    lpae_t *pte = table + pte_offset(vaddr, level);

    // hyper_debug("pte:%p = %p + %x", pte, table, pte_offset(vaddr, level));
    write_pte(pte, make_stage2_entry(paddr, (enum mem_type)attr, (enum mem_access)acc));
    if (end_level != 3)
        pte->walk.table = 0; //?

    return 0;
}

static int stage2_unmap_page(lpae_t *pre_tbl, int level, vaddr_t vir_addr) {

    paddr_t next_tbl_phy;
    lpae_t *next_tbl_vir;

    // hyper_debug("walk <%p> l-%d", vir_addr, level);
    if (!pre_tbl || level < 0 || level > 3)
        return 0;
    /* pre_tbl always well here */
    lpae_t *cur_pte = &pre_tbl[pte_offset(vir_addr, level)];

    if (cur_pte->pt.valid == 0 || cur_pte->pt.base == 0) {
        hyper_fatal("try unmap no-mapped vaddr %lx", vir_addr);
    }

    if (level == 3) {
        // hyper_debug("unmap phy: %lx", cur_pte->pt.base << 12);
        cur_pte->bits = 0;
        return 0;
    }

    next_tbl_phy = cur_pte->pt.base << 12;
    next_tbl_vir = (lpae_t *)phy_to_vir(next_tbl_phy);

    stage2_unmap_page(next_tbl_vir, level + 1, vir_addr);

    /*
     * check if all next_tbl_vir is invalid
     */
    int all_invalid = 1;
    for (int i = 0; i < 512; ++i)
        if (next_tbl_vir[i].pt.valid)
            all_invalid = 0;

    if (all_invalid) {

        // hyper_debug("debug: %d", next_tbl_vir->pt.avail);

        // hyper_debug("unmap free page 0x%lx", PHY_TO_FN(next_tbl_phy));
        free_one_page(PHY_TO_FN(next_tbl_phy));
        cur_pte->bits = 0;
    }

    return 0;
}


int stage2_map(struct stage2_mm_info *info, vaddr_t vaddr, paddr_t paddr, uint64_t map_size, int attr, int acc) {

    /* map size 4k aligned */
    lpae_t *root = (lpae_t *)info->root_table;
    int     start_level = info->lookup_level;

    map_size = (map_size + PAGE_SIZE - 1) & PAGE_MASK;

    spinlock_t *s2l = get_s2_lock();
    arch_spin_lock(s2l);
    for (u64 i = 0; i < map_size; i += PAGE_SIZE) {
        stage2_map_4k(root, start_level, vaddr, paddr, attr, acc);
        vaddr += PAGE_SIZE;
        paddr += PAGE_SIZE;
    }
    arch_spin_unlock(s2l);

    return 0;
}

int stage2_unmap(struct stage2_mm_info *info, vaddr_t vaddr, uint64_t map_size ) {
    /* map size 4k aligned */
    lpae_t *root = (lpae_t *)info->root_table;
    int     start_level = info->lookup_level;

    map_size = (map_size + PAGE_SIZE - 1) & PAGE_MASK;

    spinlock_t *s2l = get_s2_lock();
    arch_spin_lock(s2l);
    u64 start_vaddr = vaddr;
    for (u64 i = 0; i < map_size; i += PAGE_SIZE) {
        stage2_unmap_page(root, start_level, vaddr);
        vaddr += PAGE_SIZE;
    }
    arch_spin_unlock(s2l);

    /* TLB invalidation: flush stage-2 entries for the unmapped range.
     * For small ranges, flush IPA-by-IPA; for large ranges, flush all. */
    if (map_size <= (u64)32 * PAGE_SIZE) {
        for (u64 off = 0; off < map_size; off += PAGE_SIZE)
            tlb_inv_guest_ipa(start_vaddr + off);
    } else {
        tlb_inv_guest_allis();
    }

    /* Notify other pCPUs to flush their TLBs. */
    if (smp_cpu_count() > 1)
        ipi_tlb_shootdown();

    return 0;
}

int build_static_stage2_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t map_size,
                            lpae_t *table_L0, lpae_t *table_L1, lpae_t *table_L2, lpae_t *table_L3,
                            int attr) {

    lpae_t *entry = NULL;
    vaddr_t addr;

    hyper_debug("map size: %lx from %lx:%lx", map_size, virt_start, phys_start);
    build_s2_table(table_L0, (vaddr_t)table_L1, virt_start, 0, attr);
    build_s2_table(table_L1, (vaddr_t)table_L2, virt_start, 1, attr);


    addr = virt_start & (~(ARM_PT_LEVEL_SIZE(2) - 1));
    entry = &table_L2[pte_offset(virt_start, 2)];
    paddr_t next_tbl = vir_to_phy((vaddr_t)table_L3);

    do {

        *entry = make_stage2_entry(next_tbl, (enum mem_type)attr, MEM_ACCESS_RWX);
        entry->p2m.xn = 1;
        next_tbl += PAGE_SIZE;
        addr += ARM_PT_LEVEL_SIZE(2);

    } while (entry++, addr < (virt_start + map_size));


    /* fill L3 */
    paddr_t phy_start = virt_start;
    paddr_t phy_end = phy_start + map_size;
    entry = &table_L3[pte_offset(virt_start, 3)];

    if (phy_start & 0xFFF) {
        hyper_err("physical addr of level-3'page addr not aligned");
        return -1;
    }

    do {

        *entry = make_stage2_entry(phy_start, (enum mem_type)attr, MEM_ACCESS_RWX);
        phy_start += ARM_PT_LEVEL_SIZE(3);

    } while (entry++, phy_start < phy_end);

    return 0;
}

