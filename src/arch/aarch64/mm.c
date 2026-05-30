
/*
    refs xen/arch/arm/arm64/mmu/head.S
    refs linux/doc/
*/
#include "page.h"

#include "aarch64_hcr.h"
#include "vmio.h"
#include "mm.h"
#include "inline_asm.h"
#include "src/drivers/gic/gicv3.h"
#include "sys_reg.h"
#include "excep.h"
#include "hyper_config.h"

#include "mmu.h"

#include <errno.h>
#include <ioremap.h>

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

#define BOOT_MAP_SIZE (16 << 20)

DEFINE_PAGE_TABLE(boot_pgtable);
DEFINE_PAGE_TABLE(boot_first);
DEFINE_PAGE_TABLE(boot_second);
DEFINE_PAGE_TABLES(boot_fix, 512);
DEFINE_PAGE_TABLES(boot_third, BOOT_MAP_SIZE / ARM_PT_LEVEL_SIZE(2));

DEFINE_PAGE_TABLES(uart_fix_map, 1);

DEFINE_PAGE_TABLE(stage2_L0);
DEFINE_PAGE_TABLE(stage2_L1);
DEFINE_PAGE_TABLE(stage2_L2);
DEFINE_PAGE_TABLE(stage2_L2_b);
DEFINE_PAGE_TABLES(stage2_L3, BOOT_MAP_SIZE / ARM_PT_LEVEL_SIZE(2));
DEFINE_PAGE_TABLES(stage2_L3_b, 1);

#define PAGE_CNT (CONFIG_PHY_MEM_SIZE >> PAGE_SHIFT)



lpae_t *g_kmap_l2_tbl = NULL; /* after page-allocatore setup, alloc by alloc_pages() */
lpae_t *g_kmap_l3_tbl = NULL; /* for 4k table */

DEFINE_PAGE_TABLE(huge_L0);

DEFINE_PAGE_TABLE(pages_direct_mapping_L0);
DEFINE_PAGE_TABLES(pages_direct_mapping_L1, 2); /* reprent 512G space*/
DEFINE_PAGE_TABLES(pages_direct_mapping_L2, (CONFIG_PHY_MEM_SIZE) / ARM_PT_LEVEL_SIZE(1));
DEFINE_PAGE_TABLES(pages_direct_mapping_L3, BOOT_MAP_SIZE / ARM_PT_LEVEL_SIZE(2));

DEFINE_PAGE_TABLES(stage2_fix_mapping_L2, 512);

int init_direct_mapping() {
    paddr_t phy_start = PAGE_PHYS_OFFSET;
    size_t  map_size = (1ul << 30);
    paddr_t phy_end = phy_start + map_size;

    hyper_debug("page direct-mapping %lx -> %lx", phy_start, phy_end);
    print_addr_idx(PAGE_VIRT_OFFSET);
    hyper_debug("page-tables: %p,%p,%p",
              boot_pgtable,
              pages_direct_mapping_L1,
              pages_direct_mapping_L2);
    /* use two-level tables */
    __build_hyp_two_level_page_table(PAGE_VIRT_OFFSET,
                                       phy_start,
                                       map_size,
                                       7,
                                       boot_pgtable,
                                       pages_direct_mapping_L1,
                                       pages_direct_mapping_L2);
    // ptw_test(boot_pgtable, PAGE_VIRT_OFFSET);

    return 0;
}

/* 2MB one slot */
void __fix_map(vaddr_t virt, paddr_t phys, int slot, int attr, lpae_t *table_L0, lpae_t *table_L1,
               lpae_t *fix) {

    build_s2_table(table_L0, (vaddr_t)table_L1, virt, 0, attr);
    build_s2_table(table_L1, (vaddr_t)&fix[slot], virt, 1, attr);

    lpae_t e = make_stage1_entry(phys, attr);
    e.pt.table = 0;
    lpae_t *entry_start = &fix[slot];
    entry_start[pte_offset(virt, 2)] = e;
    // hyper_info("------ %p %p = %p", stage2_fix_mapping_L2, &entry_start[72], e.bits);
}

void fix_map(vaddr_t virt, paddr_t phys, int slot, int attr) {
    lpae_t *table_L0 = boot_pgtable;
    lpae_t *table_L1 = boot_first;

    __fix_map(virt, phys, slot, attr, table_L0, table_L1, boot_fix);
}

void fix_unmap(int slot) { stage2_fix_mapping_L2[slot].bits = 0; }

int build_hyper_two_level_page_table(vaddr_t virt_start, paddr_t phys_start, size_t mem_size,
                                     int attr) {

    lpae_t *table_L0 = boot_pgtable;
    lpae_t *table_L1 = boot_first;
    lpae_t *table_L2 = boot_second;

    hyper_debug("virt idx: %lx %lx %lx %lx",
              pte_offset(virt_start, 0),
              pte_offset(virt_start, 1),
              pte_offset(virt_start, 2),
              pte_offset(virt_start, 3));

    return __build_hyp_two_level_page_table(
        virt_start, phys_start, mem_size, attr, table_L0, table_L1, table_L2);
}

int build_hyper_three_level_page_table(vaddr_t virt_start, paddr_t phys_start, size_t mem_size,
                                       int attr) {

    lpae_t *table_L0 = boot_pgtable;
    lpae_t *table_L1 = boot_first;
    lpae_t *table_L2 = boot_second;
    lpae_t *table_L3 = boot_third;
    return __build_hyp_three_level_page_table(
        virt_start, phys_start, mem_size, attr, table_L0, table_L1, table_L2, table_L3);
}

void dump_stage2_table(int level) {
    hyper_info("summary: L0:%p, L1:%p, L2:%p, L3:%p", stage2_L0, stage2_L1, stage2_L2, stage2_L3);
    hyper_info("%lx", stage2_L0[0].bits);
    hyper_info("%p, %lx", boot_pgtable, boot_pgtable[0].bits);
}

/*
    L0 : static 512 Entry
    L1 : static 512 Entry  per = 4K,  so one entry= 1G, total 512G
    L2 : dynamic 512 Entry per = 4K， so one entry = 2M , total 1G
    L3 : dynamic 512 Entry per = 4K,  so one entry = 4K, total 2M

    we need check is_L2_valid & is_L3_valid

*/

lpae_t *get_next_table(lpae_t *cur_level, vaddr_t addr, int level) {
    int    idx = pte_offset(addr, level);
    lpae_t e = cur_level[idx];

    return (lpae_t *)((vaddr_t)e.p2m.base << 12);
}

void create_uart_guest_map() {

#ifdef CONFIG_BOARD_QEMU_VIRT
    paddr_t mem_start = 0x09000000;
#else
    paddr_t mem_start = 0x1C090000;
#endif

#if 1
    int attr = MEM_NORMAL_RW;
    build_s2_table(stage2_L0, (vaddr_t)stage2_L1, mem_start, 0, attr);
    build_s2_table(stage2_L1, (vaddr_t)stage2_L2_b, mem_start, 1, attr);
    build_s2_table(stage2_L2_b, (vaddr_t)stage2_L3_b, mem_start, 2, attr);

    int idx = pte_offset(mem_start, 3);
    // stage2_L2_b[72] = make_stage2_entry(stage2_L3_b, 0);
    // stage2_L3_b[0] = make_stage2_entry(mem_start, 0);//
#ifdef CONFIG_BOARD_QEMU_VIRT
    stage2_L3_b[idx].bits = 0x09000000 | 0x7ff;
#else
    stage2_L3_b[idx].bits = 0x1c090000 | 0x7ff;
#endif
    // p[idx] = make_stage2_entry(0x40200000, MEM_DEVICE);
    hyper_info("uart index: %lx %lx %lx %lx",
             pte_offset(mem_start, 0),
             pte_offset(mem_start, 1),
             pte_offset(mem_start, 2),
             pte_offset(mem_start, 3));

#endif
}

struct stage2_mm_info s2_mm_info;

/* Global lock for shared stage-2 page table operations.
 * All vCPUs share the same root_table, so we need one global lock. */
static spinlock_t g_s2_lock = { .lock = SPIN_UNLOCKED };
spinlock_t *get_s2_lock(void) { return &g_s2_lock; }

/* Saved for secondary CPU MMU bring-up (set by primary during init). */
struct mmu_boot_state g_mmu_boot;

struct stage2_mm_info *get_default_mm_info() {
    return &s2_mm_info;
}

void enable_stage2_traslation(lpae_t *table_root) {

    uint8_t pa_ps = mrs(ID_AA64MMFR0_EL1) & 0xFu;

    s2_setup_info(&s2_mm_info, pa_ps);
    s2_alloc_root_pages(&s2_mm_info);
    if(s2_mm_info.pa_size == 42)
        hyper_fatal("pa_size %d unsupported", s2_mm_info.pa_size);

    hyper_debug("pa-size:%d, ps:%d", s2_mm_info.pa_size, pa_ps);

    // stage2_map(&s2_mm_info,  MEM_VIRT_START, MEM_VIRT_START, (10 << 20), MEM_NORMAL_RW, MEM_ACCESS_RWX);
    // stage2_map(&s2_mm_info,  0x90000000, 0x90000000, (10 << 20), MEM_NORMAL_RW, MEM_ACCESS_RWX);


#ifdef CONFIG_BOARD_QEMU_VIRT
    u64 pl011_data_reg = 0x09000000;
    stage2_map(&s2_mm_info,  0x40000000, 0x40000000, (10 << 20), MEM_NORMAL_RW, MEM_ACCESS_RWX);

#else
    u64 pl011_data_reg = 0x1c090000;
    stage2_map(&s2_mm_info, 0x09000000, 0x1c090000, 0x100, MEM_DEVICE_NC, MEM_ACCESS_RWX);
#endif

    stage2_map(&s2_mm_info, CONFIG_GUEST_OS_LOAD_ADDR, CONFIG_GUEST_OS_LOAD_ADDR, (0x1<<30), MEM_NORMAL_RW, MEM_ACCESS_RWX);
    stage2_map(&s2_mm_info, 0x50000000, 0x50000000, PAGE_SIZE, MEM_NORMAL_RW, MEM_ACCESS_RWX);
    if (!hyper_config()->uart.enabled)
        stage2_map(&s2_mm_info, pl011_data_reg, pl011_data_reg, 0x100, MEM_DEVICE_NC, MEM_ACCESS_RWX);
    // stage2_unmap(&s2_mm_info, 0x09000000, 0x100);
    uint64_t val = VTCR_RES1 | VTCR_SH0_IS | VTCR_ORGN0_WBWA | VTCR_IRGN0_WBWA;
    val |= VTCR_TG0_4K;
    val |= VTCR_PS(pa_ps) | VTCR_T0SZ(64 - s2_mm_info.pa_size);
    val |= VTCR_SL0(2 - s2_mm_info.lookup_level); /* init lookup level */

    msr_sync(vtcr_el2, val);


    uint64_t vttbr_val = ((uint64_t)vir_to_phy(s2_mm_info.root_table)) & (~0xFFFUL);

    vttbr_val |= (0ul << 48);
    msr_sync(VTTBR_EL2, vttbr_val);

    /* Save for secondary CPUs */
    g_mmu_boot.vttbr_el2 = vttbr_val;
    g_mmu_boot.vtcr_el2  = val;

}
/*
 * Memory types
 */
#if 0
#define MT_DEVICE_NGNRNE	0
#define MT_DEVICE_NGNRE		1
#define MT_DEVICE_GRE		2
#define MT_NORMAL_NC		3
#define MT_NORMAL		4

#define MEMORY_ATTRIBUTES	((0x00 << (MT_DEVICE_NGNRNE * 8)) |	\
				(0x04 << (MT_DEVICE_NGNRE * 8))   |	\
				(0x0c << (MT_DEVICE_GRE * 8))     |	\
				(0x44 << (MT_NORMAL_NC * 8))      |	\
				(UL(0xff) << (MT_NORMAL * 8)))
#endif

extern void timer_init();

// #define MT_NORMAL 4

void mmu_enable(void)
{
    msr(mair_el2, 0xee0000ff440c0400);

    /* Flush local TLB */
    asm volatile("ic iallu\n\t"
                 "dsb nshst\n\t"
                 "tlbi vmalle1is\n\t"
                 "tlbi alle2\n\t"
                 "dsb nsh\n\t"
                 "isb\n\t" ::: "memory");

    /* Enable MMU + caches */
    uint64_t val = mrs(SCTLR_EL2);
    val |= (SCTLR_Axx_ELx_M | SCTLR_Axx_ELx_C);
    val &= ~(SCTLR_Axx_ELx_A);
    asm volatile("dsb sy");

    msr(SCTLR_EL2, val);
    asm volatile("isb; dsb sy; isb");
}

int ptw_test(lpae_t *tbl_root, vaddr_t vir) {
    paddr_t p = __walk_page_table(tbl_root, vir, 0) | ((vir) & 0xFFF);
    hyper_info("PTW(%lx) -> (%lx)", vir, p);

    paddr_t e = vir_to_phy(vir);

    if (p != e) {
        hyper_err("ptw failed for %lx (%lx != %lx)", vir, p, e);
        return -1;
    }

    print_addr_idx(PAGE_VIRT_OFFSET);
    hyper_info("PTW for %lx PASS", vir);
    return 0;
}
#include "tlsf.h"
void test_page_alloc() {



    /* test kmalloc/kfree */
    void *k_ptr;
    dump_kmalloc_status();
    k_ptr = kmalloc(0x1000);
    hyper_info("test kmalloc -------- %p", k_ptr);

    dump_kmalloc_status();
    kfree(k_ptr);
    dump_kmalloc_status();


    // print_page_layout(0, 512);
    int pfn2 = alloc_pages(2);
        // print_page_layout(0, 512);

    int pfn = alloc_pages(1);
            // print_page_layout(0, 512);

    hyper_info("get page:<%d vir: %lx>, <%d vir:%lx>", pfn2, PAGE_VIR(pfn2), pfn, PAGE_VIR(pfn));

    free_pages(pfn2, 2);
            // print_page_layout(0, 512);

    int pfn3 = alloc_pages(1);
    if (pfn2 != pfn3)
        hyper_fatal("page allocator Failed %d vs %d", pfn2, pfn3);


    hyper_info("page allocator test PASS");

    page_summary();
    hyper_info("kmap page count: %lx, %lx - %lx / %d",
             KMAP_TBL_PAGE_NUM,
             KMAP_VIRT_END,
             KMAP_VIRT_START,
             ARM_PT_LEVEL_SHIFT(1));

    /* init kmap */
    int fn = alloc_pages_cnt(KMAP_L2_PAGE_NUM);
    if (fn < 0)
        hyper_fatal("no enough pages:%lx for kmap", KMAP_TBL_PAGE_NUM);
    g_kmap_l2_tbl = (lpae_t *)PAGE_VIR(fn);

    fn = alloc_pages_cnt(KMAP_L3_PAGE_NUM);
    if (fn < 0)
        hyper_fatal("no engouth pages for kmap L3");
    g_kmap_l3_tbl = (lpae_t *)PAGE_VIR(fn);

#define KMAP_INVALID_ADDR 0
#if 0
    __build_hyp_two_level_page_table(KMAP_VIRT_START,
                                       KMAP_INVALID_ADDR,
                                       (KMAP_VIRT_END - KMAP_VIRT_START),
                                       MT_NORMAL,
                                       boot_pgtable,
                                       pages_direct_mapping_L1,
                                       g_kmap_l2_tbl);
#endif



    __ptw_map_4k_page(0xE100000000ul, 0x09000000, boot_pgtable, 0, MT_NORMAL);
    for (int cpu = 0; cpu < CONFIG_SMP_CPU_NUM; cpu++) {
        __ptw_map_4k_page(GICR_SGI_BASE_FIXMAP + (cpu * PAGE_SIZE),
                          hyper_config()->host_gic.gicr_base +
                              ((u64)cpu * hyper_config()->host_gic.gicr_stride) +
                              GICR_SGI_FRAME_OFFSET,
                          boot_pgtable,
                          0,
                          MT_DEVICE_nGnRnE);
    }

    // __ptw_unmap_4k_page(0xE100001000ul, boot_pgtable, 0);

    // __ptw_unmap_4k_page(0xE100000000ul, boot_pgtable, 0);

    // __ptw_map_4k_page(0xE100000000ul, 0x09000000, boot_pgtable, 0, MT_NORMAL);

    *(volatile uint64_t*)0xE100000000ul = '+';
}

void init_mm(void) {

    // size_t map_size alloced 8MB, only map real size
    size_t map_size = (paddr_t)&_hyper_end - (paddr_t)&_hyper_start;
    // MUST aligned by PAGE_SIZE


#ifdef CONFIG_HOST_USE_2MB_MAPPING
    map_size = (map_size + MB(2) - 1) & (~(MB(2) - 1));
#else
    map_size = (map_size + PAGE_SIZE - 1) & PAGE_MASK;

#endif

    // hyper_info("image size: 0x%x from %p -> %p", map_size, &_hyper_start, &_hyper_end);


    uint64_t hcr_val = get_default_hcr_flags();
    // we only support AArch64 for now
    hcr_val |= (1lu << 31);
    hcr_val |= (1lu << 42) | (1lu << 43) | (1lu << 45);
    // hcr_val |= HCR_TGE;
    // hyper_info("hcr: %x", hcr_val);
    msr(hcr_el2, hcr_val);

    safe_printf("enable mmu: %lx\n", hcr_val);
    // while(1);

    // map_size = phy_memory_size;

    // hyper_debug("dbug: from %lx %lx size:%lx", MEM_VIRT_START, MEM_VIRT_START, map_size);
#ifdef CONFIG_HOST_USE_2MB_MAPPING
    build_hyper_two_level_page_table(MEM_VIRT_START, MEM_VIRT_START, (uint64_t)map_size, MT_NORMAL_WB);
#else
    build_hyper_three_level_page_table(
        MEM_VIRT_START, MEM_VIRT_START, (uint64_t)map_size, 7);
#endif
#ifdef CONFIG_BOARD_FVP_AEMVA
    fix_map(0x1c090000, 0x1c090000, 0, MT_DEVICE_nGnRnE);
#else
    fix_map(0x09000000, 0x09000000, 0, MT_DEVICE_nGnRnE);
#endif

    msr_sync(TTBR0_EL2, boot_pgtable);
    asm volatile("isb");
    mmu_enable();


    hyper_info("mmu enabled");


    // enable_mmu(pages_direct_mapping_L0);


    (void)ptw_test(boot_pgtable, MEM_VIRT_START + map_size - 1);

    init_direct_mapping();
    init_page_allocator();
    init_kmalloc();

    init_kmap();

    test_page_alloc();

    /* setup stage2 */
    enable_stage2_traslation(stage2_L0);

    // u64 vp =  __vmalloc(0x4000);
    // u64 vp2 = __vmalloc(0x2000);
    // __vfree(vp, 0x4000);
    // u64 vp3 = __vmalloc(0x3000);
    // hyper_debug("_vmalloc get %lx, %lx %lx",vp, vp2, vp3);

#ifdef CONFIG_BOARD_QEMU_VIRT
    void *uart_addr = ioremap_page(0x09000000, MT_NORMAL);
    *(u64*)uart_addr = '^';
    iounmap_page((vaddr_t)uart_addr);
#endif

}
