#include "htypes.h"
#include "include/mm.h"
#include "compiler.h"

#include "bitmap.h"
#include "page_alloc.h"
#include "safe_printf.h"
#include "string.h"
#include "mmu.h"
#include "inline_asm.h"
#include "arch_page.h"
#include "vmio.h"

/*
story:
https://chromite.readthedocs.io/en/latest/overview.html
*/

paddr_t alloc_pg_page() {
    int fn = alloc_pages(0);
    return fn;
}

static inline int pte_offset(u64 addr, int level) {
    return (addr >> PAGE_LEVEL_SHIFT(level) & (PTRS_PER_ENTRY - 1));
}


ptw_t make_pg_entry(u64 phy_addr, int attr) {
    ptw_t ret;

    memset(&ret, 0, sizeof(ret));

    ret.walk.base = phy_addr >> 12;
    ret.walk.valid = 1;
    // ret.walk.pbmt = 1;

    return ret;
}

void write_pte(ptw_t *ventry, paddr_t addr, int attr) {
    ptw_t pte = make_pg_entry(addr, attr);

    if (attr & PAGE_ATTR_READ)
        pte.walk.read = 1;

    if (attr & PAGE_ATTR_WRITE) {
        pte.walk.write = 1;
        pte.walk.read = 1;
    }

    if (attr & PAGE_ATTR_EXEC) {
        /* x|w is reserved */
        pte.walk.exec = 1;
    }

    if (attr & PAGE_ATTR_USER)
        pte.walk.user = 1;

    // pte.walk.access = 1;
    // pte.walk.user = 1;

    *(ptw_t *)ventry = pte;
}

static inline int pte_is_valid(ptw_t *entry) {
    return entry->walk.valid == 1; }

static int pg_next_level(ptw_t **vtable, unsigned int level, unsigned int offset, int attr, int without_mmu) {

    ptw_t *entry;

    entry = *vtable + offset;

    if (level == 3 && pte_is_valid(entry)) {
        safe_printf("try remap existed entry\n");
    }

    if (!pte_is_valid(entry)) {
        if (level != 3) {
            int fn = alloc_pg_page();
            // safe_printf("alloc s2 page 0x%lx v:%p\n", fn, PAGE_VIR(fn));
            if (fn < 0) {
                safe_printf("out-of-memory for ptw map\n");
            }
            if (without_mmu)
                memset((void *)PAGE_PHY(fn), 0, PAGE_SIZE);
            else
                memset((void *)PAGE_VIR(fn), 0, PAGE_SIZE);

            *entry = make_pg_entry(PAGE_PHY(fn), attr);
            // alloc a page for pte
        } else {
            // *entry = make_stage2_entry(, attr);
            safe_printf("should NOT be here\n");
        }
    }

    /* block desc */
    if (is_leaf(entry)) {
        safe_printf("walk block desc\n");
        return 0;
    }

    /*
     table -> vaddr_t
     unmap *table first, when disable direct-mapping
    */
    if(without_mmu)
        *vtable = (ptw_t *)(uintptr_t)(entry->walk.base << 12);
    else
        *vtable = (ptw_t *)(uintptr_t)phy_to_vir(entry->walk.base << 12);

    return 0;
}

static int __pg_map(ptw_t *root, int start_level, int end_level, vaddr_t vaddr, paddr_t paddr, int attr, int without_mmu) {
    ptw_t *table = NULL;
    int    level;

    table = root;

    for (level = start_level; level < end_level; ++level) {
        // hyper_info("table:%p", table);
        pg_next_level(&table, level, pte_offset(vaddr, level), 0, without_mmu);
    }

    // map last pte
    ptw_t *pte = table + pte_offset(vaddr, level);

    // hyper_debug("pte:%p = %p + %x", pte, table, pte_offset(vaddr, level));
    write_pte(pte, paddr, attr);

    return 0;
}

static int pg_unmap_4k(ptw_t *pre_tbl, int level, vaddr_t vir_addr) {

    paddr_t next_tbl_phy;
    ptw_t *next_tbl_vir;

    // hyper_debug("walk <%p> l-%d", vir_addr, level);
    if (!pre_tbl || level < 0 || level > 3)
        return 0;
    /* pre_tbl always well here */
    ptw_t *cur_pte = &pre_tbl[pte_offset(vir_addr, level)];

    if (cur_pte->walk.valid == 0 || cur_pte->walk.base == 0) {
        hyper_fatal("try unmap no-mapped vaddr %lx", vir_addr);
    }

    if (level == 3) {
        // hyper_debug("unmap phy: %lx", cur_pte->pt.base << 12);
        cur_pte->bits = 0;
        return 0;
    }

    next_tbl_phy = cur_pte->walk.base << 12;
    next_tbl_vir = (ptw_t *)phy_to_vir(next_tbl_phy);

    pg_unmap_4k(next_tbl_vir, level + 1, vir_addr);

    /*
     * check if all next_tbl_vir is invalid
     */
    int all_invalid = 1;
    for (int i = 0; i < 512; ++i)
        if (next_tbl_vir[i].walk.valid)
            all_invalid = 0;

    if (all_invalid) {

        // hyper_debug("debug: %d", next_tbl_vir->pt.avail);

        // hyper_debug("unmap free page 0x%lx", PHY_TO_FN(next_tbl_phy));
        free_one_page(PHY_TO_FN(next_tbl_phy));
        cur_pte->bits = 0;
    }
}

int pg_map_4k(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, int attr, int without_mmu) {
    return __pg_map(root, start_level, 3, vaddr, paddr, attr, without_mmu);
}

int pg_map_2MB(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, int attr, int without_mmu) {
    return __pg_map(root, start_level, 2, vaddr, paddr, attr, without_mmu);
}

int pg_map_1GB(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, int attr, int without_mmu) {
    return __pg_map(root, start_level, 1, vaddr, paddr, attr, without_mmu);
}

int pg_map(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, uint64_t map_size,
           int attr, int without_mmu) {

    /* map size 4k aligned */
    map_size = (map_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    for (u64 i = 0; i < map_size; i += PAGE_SIZE) {
        pg_map_4k(root, start_level, vaddr, paddr, attr, without_mmu);
        vaddr += PAGE_SIZE;
        paddr += PAGE_SIZE;
    }

    return 0;
}

int pg_unmap(ptw_t *root, int start_level, vaddr_t vaddr, uint64_t map_size) {

    /* map size 4k aligned */
    map_size = (map_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    for (u64 i = 0; i < map_size; i += PAGE_SIZE) {
        pg_unmap_4k(root, start_level, vaddr);
        vaddr += PAGE_SIZE;
    }

    return 0;
}

extern ptw_t page_table_root[512 * 4];
extern ptw_t page_stage2_table_root[512 * 4];

int pg_map_huge(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, uint64_t map_size,
           int attr, int without_mmu) {

    vaddr = vaddr & ~(MB_2 - 1);
    paddr = paddr & ~(MB_2 - 1);
    map_size = (map_size + MB_2 - 1) & (~(MB_2 - 1));

    for (u64 i = 0; i < map_size; i += MB_2) {
        pg_map_2MB(root, start_level, vaddr, paddr, attr, without_mmu);
        vaddr += MB_2;
        paddr += MB_2;
    }

    return 0;
}

int host_map_one_page(vaddr_t vir, paddr_t phy, int attr){
    pg_map(page_table_root, 0, vir, phy, PAGE_SIZE, attr, 0);
}

//FIXME
void host_unmap_one_page(vaddr_t vir) { pg_unmap_4k(page_table_root, 0, vir); }

int pg_map_stage2(vaddr_t vaddr, paddr_t paddr, uint64_t map_size, int attr, int without_mmu) {
    int ret = pg_map(page_stage2_table_root, 0, vaddr, paddr, map_size, attr, without_mmu);
    return ret;
}

int stage2_map(struct stage2_mm_info *info, vaddr_t vaddr, paddr_t paddr, uint64_t map_size, int attr, int acc) {

    return pg_map((ptw_t *)info->root_table, 0, vaddr, paddr, attr, map_size, 0);
}

int stage2_unmap(struct stage2_mm_info *info, vaddr_t vaddr, uint64_t map_size) {
    return pg_unmap((ptw_t *)info->root_table, 0, vaddr, map_size);
}

paddr_t ptw_walk(ptw_t *cur_tbl, vaddr_t addr, int level) {

    paddr_t next_tbl_phy;
    ptw_t  *next_tbl_vir;


    if (!cur_tbl || level < 0 || level > 3){
        safe_printf("dfs done with: %lx, level:%d\n", cur_tbl, level);
        return 0;
    }

    ptw_t next_pte = cur_tbl[pte_offset(addr, level)];
    next_tbl_phy = next_pte.walk.base << 12;

    if (next_pte.walk.valid == 0) {
        safe_printf("invalid pte, %lx\n", pte_offset(addr, level));
        return 0;
    }



    safe_printf("ptw cur:%lx(off:%lx = %lx) -> %lx\n",
                cur_tbl,
                pte_offset(addr, level),
                next_pte,
                next_tbl_phy);
    if (level == 3) {
        safe_printf("last level\n");
        return next_tbl_phy;
    }

    next_tbl_vir = (ptw_t *)phy_to_vir(next_tbl_phy);

    return ptw_walk(next_tbl_vir, addr, level + 1);
}

int enable_mmu(uint64_t pg_root) {
    safe_printf("stap: %lx\n", pg_root);
    __sfence_vma_all();
    csrw(satp, pg_root);

    return 0;
}
