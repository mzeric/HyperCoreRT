
/*
    refs xen/arch/arm/arm64/mmu/head.S
    refs linux/doc/
*/
#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "cpu_inline_asm.h"

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

#define PT_PT     0xf7f /* nG=1 AF=1 SH=11 AP=01 NS=1 ATTR=111 T=1 P=1 */
#define PT_MEM    0xf7d /* nG=1 AF=1 SH=11 AP=01 NS=1 ATTR=111 T=0 P=1 */
#define PT_MEM_L3 0xf7f /* nG=1 AF=1 SH=11 AP=01 NS=1 ATTR=111 T=1 P=1 */
#define PT_DEV    0xe71 /* nG=1 AF=1 SH=10 AP=01 NS=1 ATTR=100 T=0 P=1 */
#define PT_DEV_L3 0xe73 /* nG=1 AF=1 SH=10 AP=01 NS=1 ATTR=100 T=1 P=1 */

DEFINE_PAGE_TABLE(boot_pgtable);
DEFINE_PAGE_TABLE(boot_first);
DEFINE_PAGE_TABLE(boot_second);
DEFINE_PAGE_TABLES(boot_third, (8 << 20) / ARM_PT_LEVEL_SIZE(2));

DEFINE_PAGE_TABLES(uart_fix_map, (1 << 20) / ARM_PT_LEVEL_SIZE(2));

uint64_t get_table_slot(paddr_t virt, uint8_t level) {
    uint64_t mask = (1UL << ARM_PT_LPAE_SHIFT) - 1;
    uint64_t val = (virt >> ARM_PT_LEVEL_SHIFT(level)) & mask;
    return val;
}

uint64_t vir_to_phy(uint64_t v) {
    return v;
}

void create_table_entry_from_paddr(uint64_t *ptbl, paddr_t tbl, paddr_t virt, uint8_t level) {
    int entry_idx = get_table_slot(virt, level);

    // vmm_debug("set %p[%d] = %x\n", ptbl, entry_idx, (tbl | PT_PT));
    ptbl[entry_idx] = (tbl | PT_PT);
}

void create_table_entry(uint64_t *ptbl, vaddr_t tbl, vaddr_t virt, uint8_t level) {
    paddr_t addr = vir_to_phy(tbl);
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

void create_boot_page_tables(
        vaddr_t virt_addr, paddr_t phys_addr, size_t map_size) {

    create_table_entry(boot_pgtable, boot_first, virt_addr, 0);
    create_table_entry(boot_first, boot_second, virt_addr, 1);

    int cnt = (8 << 20) / ARM_PT_LEVEL_SIZE(2);
    vmm_info("debug:%x, %d\n", ARM_PT_LEVEL_SIZE(2), cnt);
    vaddr_t addr = virt_addr;
    for (int i = 0; i < cnt; ++i) {
        /* setup L1(second) entrys */
        paddr_t val = vir_to_phy(boot_third);
        create_table_entry_from_paddr(boot_second, val, addr, 2);
        val += ARM_PT_LEVEL_SIZE(2);
        val += PAGE_SIZE;
    }

    int page_cnt = (map_size >> PAGE_SHIFT);

    paddr_t text_load_start = phys_addr;
    paddr_t phy_addr =
            ((text_load_start >> THIRD_SHIFT) << THIRD_SHIFT) | PT_MEM_L3;

    uint64_t* entry_p = (uint64_t*)boot_third;

    vmm_debug("page_cnt:%d\n", page_cnt);
    for (int i = 0; i < page_cnt; ++i) {
        // vmm_debug("set l3 %p[%d] = %x\n", boot_third_p, i, x2);
        entry_p[i] = phy_addr;
        phy_addr += PAGE_SIZE;
    }

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
    enable_mmu(boot_pgtable);
}