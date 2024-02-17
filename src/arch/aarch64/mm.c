
/*
    refs xen/arch/arm/arm64/mmu/head.S
    refs linux/doc/
*/
#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "cpu_inline_asm.h"
#include <errno.h>

/*

Translation table lookup with 4KB pages:

+--------+--------+--------+--------+--------+--------+--------+--------+
|63    56|55    48|47    40|39    32|31    24|23    16|15     8|7      0|
+--------+--------+--------+--------+--------+--------+--------+--------+
 |                 |         |         |         |         |
 |                 |         |         |         |         v
 |                 |         |         |         |   [11:0]  in-page offset
 |                 |         |         |         +-> [20:12] L3 index
 |                 |         |         +-----------> [29:21] L2 index
 |                 |         +---------------------> [38:30] L1 index
 |                 +-------------------------------> [47:39] L0 index
 +-------------------------------------------------> [63] TTBR0/1

*/

/*

    Translation table format D4.2.7


*/

#define PT_PT     0xf7f /* nG=1 AF=1 SH=11 AP=01 NS=1 ATTR=111 T=1 P=1 */
#define PT_MEM    0xf7d /* nG=1 AF=1 SH=11 AP=01 NS=1 ATTR=111 T=0 P=1 */
#define PT_MEM_L3 0xf7f /* nG=1 AF=1 SH=11 AP=01 NS=1 ATTR=111 T=1 P=1 */
#define PT_DEV    0xe71 /* nG=1 AF=1 SH=10 AP=01 NS=1 ATTR=100 T=0 P=1 */
#define PT_DEV_L3 0xe73 /* nG=1 AF=1 SH=10 AP=01 NS=1 ATTR=100 T=1 P=1 */

DEFINE_PAGE_TABLE(boot_pgtable);
DEFINE_PAGE_TABLE(boot_first);
DEFINE_PAGE_TABLE(boot_second);
DEFINE_PAGE_TABLES(boot_third, (8 << 20) / ARM_PT_LEVEL_SIZE(2));

DEFINE_PAGE_TABLES(uart_fix_map, 1);

DEFINE_PAGE_TABLE(stage2_L0);
DEFINE_PAGE_TABLE(stage2_L1);
// DEFINE_PAGE_TABLE(stage2_L1_b);
DEFINE_PAGE_TABLE(stage2_L2);
DEFINE_PAGE_TABLE(stage2_L2_b);
DEFINE_PAGE_TABLES(stage2_L3, (8 << 20) / ARM_PT_LEVEL_SIZE(2));
DEFINE_PAGE_TABLES(stage2_L3_b, (1 << 20) / ARM_PT_LEVEL_SIZE(2));


uint64_t get_table_entry_idx(vaddr_t virt, uint8_t level) {
    uint64_t mask = (1UL << ARM_PT_LPAE_SHIFT) - 1;
    uint64_t val = (virt >> ARM_PT_LEVEL_SHIFT(level)) & mask;
    return val;
}

paddr_t vir_to_phy(vaddr_t v) {
    return (paddr_t)v;
}

void create_table_entry_from_paddr(uint64_t *ptbl, paddr_t next_tbl, paddr_t virt, uint8_t level) {
    int entry_idx = get_table_entry_idx(virt, level);

    ptbl[entry_idx] = (next_tbl | PT_PT);
}

void build_p2m_table(lpae_t *table_current_level, vaddr_t next_tbl, vaddr_t virt, uint8_t level) {
    int idx = get_table_entry_idx(virt, level);
    paddr_t addr = vir_to_phy(next_tbl);
    table_current_level[idx] = make_p2m_table_entry(addr);
    // vmm_debug("%p[%d] = %p[0x%x]\n", table_current_level, idx, addr, table_current_level[idx].bits);
}

void build_stage2_page_table(vaddr_t mem_start, size_t map_size, lpae_t *L0, lpae_t *L1, lpae_t *L2, lpae_t *L3) {
    build_p2m_table(L0, L1, mem_start, 0);
    build_p2m_table(L1, L2, mem_start, 1);


    int cnt = (8 << 20) / ARM_PT_LEVEL_SIZE(2);
    vmm_info("debug:%x, %d\n", ARM_PT_LEVEL_SIZE(2), cnt);
    vaddr_t addr = mem_start;
    paddr_t val = vir_to_phy(L3);
    for (int i = 0; i < cnt; ++i) {
        /* setup L1(second) entrys */
        build_p2m_table(L2, val, addr, 2);
        val += PAGE_SIZE;
        addr += ARM_PT_LEVEL_SIZE(2);
    }

    /* fill L3: first 2MN */
    cnt = map_size >> PAGE_SHIFT;

    // paddr_t phy_start =            ((MEM_VIRT_START >> THIRD_SHIFT) << THIRD_SHIFT) | PT_MEM_L3;
    paddr_t phy_start = (mem_start >> THIRD_SHIFT) << THIRD_SHIFT;
    lpae_t* entry = L3;
    for (int i = 0; i < cnt; ++i) {
        // build_p2m_table(stage2_L3, phy_start, phy_start, 3);
        lpae_t p = make_p2m_table_entry(phy_start);
        entry[i] = p;
        phy_start += PAGE_SIZE;
    }
}

void create_table_entry(uint64_t *ptbl, vaddr_t next_tbl, vaddr_t virt, uint8_t level) {
    paddr_t addr = vir_to_phy(next_tbl);
    create_table_entry_from_paddr(ptbl, addr, virt, level);
}

void create_uart_fix_map() {
    paddr_t uart_phy_addr = 0x09000000;
    vaddr_t uart_vir_addr = 0x09000000;

    create_table_entry(boot_pgtable, boot_first, uart_vir_addr, 0);
    create_table_entry(boot_first, boot_second, uart_vir_addr, 1);
    create_table_entry(boot_second, &uart_fix_map[0], uart_vir_addr, 2);

    uint64_t *p = uart_fix_map;

    p[0] = uart_phy_addr | PT_MEM_L3;

}


/*
{
    x0 = x4
                boot_second x0, x1, 2, x2, x3
    x1 += 2M;
    x4 += PAGE_SIZE (tbl)
}

*/
void create_boot_page_tables(
        vaddr_t virt_addr, paddr_t phys_addr, size_t map_size) {

    create_table_entry(boot_pgtable, boot_first, virt_addr, 0);
    create_table_entry(boot_first, boot_second, virt_addr, 1);

    int cnt = (8 << 20) / ARM_PT_LEVEL_SIZE(2);
    vmm_info("debug:%x, %d\n", ARM_PT_LEVEL_SIZE(2), cnt);
    vaddr_t addr = virt_addr;
    paddr_t val = vir_to_phy(boot_third);
    // cnt = 1;
    for (int i = 0; i < cnt; ++i) {
        /* setup L2(second) entrys */
        create_table_entry_from_paddr(boot_second, val, addr, 2);
        val += PAGE_SIZE;
        addr += ARM_PT_LEVEL_SIZE(2);
    }

    int page_cnt = (map_size >> PAGE_SHIFT);

    paddr_t text_load_start = phys_addr;
    paddr_t phy_addr =
            ((text_load_start >> THIRD_SHIFT) << THIRD_SHIFT) | PT_MEM_L3;

    uint64_t* entry_p = (uint64_t*)boot_third;

    vmm_debug("page_cnt:%d\n", page_cnt);
    for (int i = 0; i < page_cnt; ++i) {

        entry_p[i] = phy_addr;
        // create_table_entry_from_paddr(boot_third, phy_addr, phy_addr, 3);
        phy_addr += PAGE_SIZE;
    }
    vmm_debug("set done\n");

    create_uart_fix_map();
}

lpae_t make_lpae_entry(mfn_t mfn, unsigned int attr)
{
    lpae_t e = (lpae_t) {
        .pt = {
            .valid = 1,           /* Mappings are present */
            .table = 0,           /* Set to 1 for links and 4k maps */
            .ai = attr,
            .ns = 1,              /* Hyp mode is in the non-secure world */
            .up = 1,              /* See below */
            .ro = 0,              /* Assume read-write */
            .af = 1,              /* No need for access tracking */
            .ng = 1,              /* Makes TLB flushes easier */
            .contig = 0,          /* Assume non-contiguous */
            .xn = 1,              /* No need to execute outside .text */
            .avail = 0,           /* Reference count for domheap mapping */
        }};
    /*
     * For EL2 stage-1 page table, up (aka AP[1]) is RES1 as the translation
     * regime applies to only one exception level (see D4.4.4 and G4.6.1
     * in ARM DDI 0487B.a). If this changes, remember to update the
     * hard-coded values in head.S too.
     */

    switch ( attr )
    {
    case MT_NORMAL_NC:
        /*
         * ARM ARM: Overlaying the shareability attribute (DDI
         * 0406C.b B3-1376 to 1377)
         *
         * A memory region with a resultant memory type attribute of Normal,
         * and a resultant cacheability attribute of Inner Non-cacheable,
         * Outer Non-cacheable, must have a resultant shareability attribute
         * of Outer Shareable, otherwise shareability is UNPREDICTABLE.
         *
         * On ARMv8 sharability is ignored and explicitly treated as Outer
         * Shareable for Normal Inner Non_cacheable, Outer Non-cacheable.
         */
        e.pt.sh = LPAE_SH_OUTER;
        break;
    case MT_DEVICE_nGnRnE:
    case MT_DEVICE_nGnRE:
        /*
         * Shareability is ignored for non-Normal memory, Outer is as
         * good as anything.
         *
         * On ARMv8 sharability is ignored and explicitly treated as Outer
         * Shareable for any device memory type.
         */
        e.pt.sh = LPAE_SH_OUTER;
        break;
    default:
        e.pt.sh = LPAE_SH_INNER;  /* Xen mappings are SMP coherent */
        break;
    }

    WARN_ON((mfn_to_maddr(mfn) & ~PADDR_MASK));

    lpae_set_mfn(e, mfn);

    return e;
}

/**********
 *
 * P2M
*/


static void p2m_set_permission(lpae_t *e, p2m_type_t t, p2m_access_t a)
{
    /* First apply type permissions */
    switch ( t )
    {
    case p2m_ram_rw:
        e->p2m.xn = 0;
        e->p2m.write = 1;
        break;

    case p2m_ram_ro:
        e->p2m.xn = 0;
        e->p2m.write = 0;
        break;

    case p2m_iommu_map_rw:
    case p2m_map_foreign_rw:
    case p2m_grant_map_rw:
    case p2m_mmio_direct_dev:
    case p2m_mmio_direct_nc:
    case p2m_mmio_direct_c:
        e->p2m.xn = 1;
        e->p2m.write = 1;
        break;

    case p2m_iommu_map_ro:
    case p2m_map_foreign_ro:
    case p2m_grant_map_ro:
    case p2m_invalid:
        e->p2m.xn = 1;
        e->p2m.write = 0;
        break;

    case p2m_max_real_type:
        // BUG();
        break;
    }

    /* Then restrict with access permissions */
    switch ( a )
    {
    case p2m_access_rwx:
        break;
    case p2m_access_wx:
        e->p2m.read = 0;
        break;
    case p2m_access_rw:
        e->p2m.xn = 1;
        break;
    case p2m_access_w:
        e->p2m.read = 0;
        e->p2m.xn = 1;
        break;
    case p2m_access_rx:
    case p2m_access_rx2rw:
        e->p2m.write = 0;
        break;
    case p2m_access_x:
        e->p2m.write = 0;
        e->p2m.read = 0;
        break;
    case p2m_access_r:
        e->p2m.write = 0;
        e->p2m.xn = 1;
        break;
    case p2m_access_n:
    case p2m_access_n2rwx:
        e->p2m.read = e->p2m.write = 0;
        e->p2m.xn = 1;
        break;
    }
}

static lpae_t mfn_to_p2m_entry(mfn_t mfn, p2m_type_t t, p2m_access_t a)
{
    /*
     * sh, xn and write bit will be defined in the following switches
     * based on mattr and t.
     */

    lpae_t e = (lpae_t) {
        .p2m.af = 1,
        .p2m.read = 1,
        .p2m.table = 1,
        .p2m.valid = 1,
        // .p2m.type = 1,
        .p2m.sbz1 = 0,
    };


    // BUILD_BUG_ON(p2m_max_real_type > (1 << 4));

    switch ( t )
    {
    case p2m_mmio_direct_dev:
        e.p2m.mattr = MATTR_DEV;
        e.p2m.sh = LPAE_SH_OUTER;
        break;

    case p2m_mmio_direct_c:
        e.p2m.mattr = MATTR_MEM;
        e.p2m.sh = LPAE_SH_OUTER;
        break;

    /*
     * ARM ARM: Overlaying the shareability attribute (DDI
     * 0406C.b B3-1376 to 1377)
     *
     * A memory region with a resultant memory type attribute of Normal,
     * and a resultant cacheability attribute of Inner Non-cacheable,
     * Outer Non-cacheable, must have a resultant shareability attribute
     * of Outer Shareable, otherwise shareability is UNPREDICTABLE.
     *
     * On ARMv8 shareability is ignored and explicitly treated as Outer
     * Shareable for Normal Inner Non_cacheable, Outer Non-cacheable.
     * See the note for table D4-40, in page 1788 of the ARM DDI 0487A.j.
     */
    case p2m_mmio_direct_nc:
        e.p2m.mattr = MATTR_MEM_NC;
        e.p2m.sh = LPAE_SH_OUTER;
        break;

    default:
        e.p2m.mattr = MATTR_MEM;
        e.p2m.sh = LPAE_SH_INNER;
    }

    p2m_set_permission(&e, t, a);

    // ASSERT(!(mfn_to_maddr(mfn) & ~PADDR_MASK));

    lpae_set_mfn(e, mfn);

    return e;
}

lpae_t make_p2m_table_entry(vaddr_t virt) {

    return mfn_to_p2m_entry(virt >> PAGE_SHIFT, p2m_ram_rw, p2m_access_rwx);
}

void dump_stage2_table(int level) {
    vmm_info("summary: L0:%p, L1:%p, L2:%p, L3:%p\n", stage2_L0, stage2_L1, stage2_L2, stage2_L3);
    vmm_info("%p\n", stage2_L0[0]);
    vmm_info("%p, %p\n",boot_pgtable, boot_pgtable[0]);
}

/*
    L0 : static 512 Entry
    L1 : static 512 Entry  per = 4K,  so one entry= 1G, total 512G
    L2 : dynamic 512 Entry per = 4K， so one entry = 2M , total 1G
    L3 : dynamic 512 Entry per = 4K,  so one entry = 4K, total 2M

    we need check is_L2_valid & is_L3_valid

*/
#define ENTRY_VALID 1

lpae_t* get_next_table(lpae_t* cur_level, vaddr_t addr, int level) {
    int idx = get_table_entry_idx(addr, level);
    lpae_t e = cur_level[idx];
    return (lpae_t*)((vaddr_t)e.p2m.base << 12);
}

int is_L0_valid(lpae_t *root, vaddr_t addr){
    MARK_UNUSED(addr);
    return ENTRY_VALID;
}

int is_L1_valid(lpae_t *root, vaddr_t addr){
    lpae_t* L1_table = get_next_table(root, addr, 0);
    int idx = get_table_entry_idx(addr, 1);

    return L1_table[idx].p2m.valid;
}

int is_L2_valid(lpae_t *root, vaddr_t addr) {
    lpae_t* L1_table = get_next_table(root, addr, 0);
    int idx = get_table_entry_idx(addr, 2);
}

int map_ipa_to_phys(lpae_t *ttbl, vaddr_t ipa, paddr_t phys) {

    return 0;
}


void enable_p2m(lpae_t *table_root) {
    build_stage2_page_table(MEM_VIRT_START, (8<<20), stage2_L0, stage2_L1, stage2_L2, stage2_L3);
    build_stage2_page_table(0x09000000, PAGE_SIZE, stage2_L0, stage2_L1, stage2_L2_b, stage2_L3_b);

    uint64_t val = VTCR_RES1|VTCR_SH0_IS|VTCR_ORGN0_WBWA|VTCR_IRGN0_WBWA;
    val |= VTCR_TG0_4K;
    val |= VTCR_PS(4) |VTCR_T0SZ(64-44);
    val |= VTCR_SL0(2);

    vmm_info("VTCR_EL2:%x\n", val);
    msr_sync(vtcr_el2, val);
    void *ptr = malloc(4000);
    void *ptr2 = memalign(0x1000, 0x1000);
    vmm_info("ptr:%p, ptr2:%p\n", ptr, ptr2);
    free(ptr);free(ptr2);

    uint64_t vttbr_val = (uint64_t)stage2_L0 & (~0xFFFUL);
    vttbr_val |= (0<<48);

#define INIT_ZERO_ADDR
#ifdef INIT_ZERO_ADDR
    /* init zero paging to capture NULL pointer issue */
    vmm_debug("stage_L2_b:%p\n", stage2_L2_b);

    // stage2_L0[0].bits = 0;
    // stage2_L1[0].bits = 0;
    void *entry_p = stage2_L2_b;
    stage2_L1[0].bits = (uint64_t)entry_p | 0x7ff;
    size_t addr = stage2_L3_b;
    for(int i = 0; i < 1; ++i){
        stage2_L2_b[i].bits = (uint64_t)addr | 0x7ff;
        addr += PAGE_SIZE;
    }

    /* 0 addr */

    stage2_L3_b[0].bits = 0x40100000|0x7ff;
    vmm_info("%x\n", stage2_L1[1].bits);
#endif
    dump_stage2_table(0);
    // vttbr_val = &enable_p2m;
    msr_sync(VTTBR_EL2, vttbr_val);
}


void enable_mmu(void* table) {
    vmm_info("Enable paging\n");
    /* flush local TLB */
    asm volatile("dsb nshst\n\t"
                 "tlbi alle2\n\t"
                 "dsb nsh\n\t"
                 "isb\n\t" ::
                         : "memory");

    msr_sync(TTBR0_EL2, table);

    uint64_t val = mrs(SCTLR_EL2);
    vmm_debug("mmu: %x\n", val);

    /* enable mmu and data cache */
    val |= (SCTLR_Axx_ELx_M | SCTLR_Axx_ELx_C);

    msr_sync(SCTLR_EL2, val);

    asm volatile("b 1f\n\t1:\n\t");
    vmm_debug("enable mmu done\n");
}


void init_mm(void) {

    extern void *_hyper_start, *_hyper_end;
    // size_t map_size alloced 8MB, only map real size
    size_t map_size = (paddr_t)&_hyper_end - (paddr_t)&_hyper_start;
    // MUST aligned by PAGE_SIZE
    map_size = (map_size + PAGE_SIZE - 1 ) & PAGE_MASK;

    create_boot_page_tables(MEM_VIRT_START, MEM_VIRT_START, map_size);

    asm volatile("msr sctlr_el1, xzr");

    uint32_t hcr_val = get_default_hcr_flags();
    //we only support AArch64 for now
    hcr_val |= (1 << 31);
    vmm_info("hcr: %x\n", hcr_val);
    msr(hcr_el2, hcr_val);

    /* setup stage2 */
    // uint64_t
    enable_p2m(stage2_L0);



    enable_mmu(boot_pgtable);

}
