
/*
    refs xen/arch/arm/arm64/mmu/head.S
    refs linux/doc/
*/
#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "cpu_inline_asm.h"
#include "execp.h"
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

#define PAGE_CNT (CONFIG_PHY_MEM_SIZE>>PAGE_SHIFT)

#define DIRECT_MAPPING_VIRT_START (0xF00UL << 32) /* 0xF00_0000_0000 */
#define DIRECT_MAPPING_VIRT_END (0xFF0UL << 32) /* 0xFF0_0000_0000 */

#define PHYS_OFFSET CONFIG_ENTRY_ADDR
#define VIRT_OFFSET DIRECT_MAPPING_VIRT_END

#define VIRT_TO_PHYS(addr) (((addr)-VIRT_OFFSET) + PHYS_OFFSET)

DEFINE_PAGE_TABLE(stage2_direct_mapping_L0);
DEFINE_PAGE_TABLES(stage2_direct_mapping_L1, (CONFIG_PHY_MEM_SIZE)/ ARM_PT_LEVEL_SIZE(1)); /* reprent 512G space*/
DEFINE_PAGE_TABLES(stage2_direct_mapping_L2, (CONFIG_PHY_MEM_SIZE)/ ARM_PT_LEVEL_SIZE(1));
DEFINE_PAGE_TABLES(stage2_direct_mapping_L3, BOOT_MAP_SIZE / ARM_PT_LEVEL_SIZE(2));
DEFINE_PAGE_TABLES(stage2_fix_mapping_L2, 512);


/* 2MB one slot */
void __fix_map(vaddr_t virt, paddr_t phys, int slot, int attr, lpae_t *table_L0, lpae_t *table_L1, lpae_t *fix) {

    build_s2_table(table_L0, (vaddr_t)table_L1, virt, 0, attr);
    build_s2_table(table_L1, (vaddr_t)&fix[slot], virt, 1, attr);

    lpae_t e = make_lpae_entry(phys >> 12, attr);
    e.pt.table = 0;
    lpae_t *entry_start = &fix[slot];
    entry_start[pte_offset(virt, 2)] = e;
    // vmm_info("------ %p %p = %p\n", stage2_fix_mapping_L2, &entry_start[72], e.bits);
}

void fix_map(vaddr_t virt, paddr_t phys, int slot, int attr) {
    lpae_t* table_L0 = boot_pgtable;
    lpae_t* table_L1 = boot_first;

    __fix_map(virt, phys, slot, attr, table_L0, table_L1, boot_fix);
}

void fix_unmap(int slot) {
    stage2_fix_mapping_L2[slot].bits = 0;
}


int build_hyper_two_level_page_table(vaddr_t virt_start, paddr_t phys_start, size_t mem_size, int attr) {

    lpae_t *table_L0 = boot_pgtable;
    lpae_t *table_L1 = boot_first;
    lpae_t *table_L2 = boot_second;

    return __build_hyper_two_level_page_table(virt_start, phys_start, mem_size,
            attr, table_L0, table_L1, table_L2);
}

int build_hyper_three_level_page_table(vaddr_t virt_start, paddr_t phys_start, size_t mem_size, int attr) {

    lpae_t* table_L0 = boot_pgtable;
    lpae_t* table_L1 = boot_first;
    lpae_t* table_L2 = boot_second;
    lpae_t* table_L3 = boot_third;
    return __build_hyper_three_level_page_table(virt_start, phys_start, mem_size,
            attr, table_L0, table_L1, table_L2, table_L3);
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
    int idx = pte_offset(addr, level);
    lpae_t e = cur_level[idx];
    return (lpae_t*)((vaddr_t)e.p2m.base << 12);
}

void create_uart_guest_map() {
    paddr_t mem_start = 0x09000000;
#if 1
    int attr = p2m_ram_rw;
    build_s2_table(stage2_L0, (vaddr_t)stage2_L1, mem_start, 0, attr);
    build_s2_table(stage2_L1, (vaddr_t)stage2_L2_b, mem_start, 1, attr);
    build_s2_table(stage2_L2_b, (vaddr_t)stage2_L3_b, mem_start, 2, attr);

    int idx = pte_offset(mem_start, 3);
    // stage2_L2_b[72] = make_p2m_table_entry(stage2_L3_b, 0);
    // stage2_L3_b[0] = make_p2m_table_entry(mem_start, 0);//
    stage2_L3_b[idx].bits = 0x09000000 | 0x7ff;
    // p[idx] = make_p2m_table_entry(0x40200000, p2m_mmio_direct_dev);
    *(volatile unsigned long*)0x40200000 = 0xbeaf;
    vmm_info("XXX:%p, %p\n", stage2_L2_b[72].bits, stage2_L3_b);
    vmm_info("uart index: %d %d %d %d\n", pte_offset(mem_start, 0),
            pte_offset(mem_start, 1),
            pte_offset(mem_start, 2),
            pte_offset(mem_start, 3));
#endif
}

void enable_stage2_traslation(lpae_t *table_root) {

    build_stage2_page_table(MEM_VIRT_START, MEM_VIRT_START, (10 << 20),
            stage2_L0, stage2_L1, stage2_L2, stage2_L3, p2m_ram_rw);

    create_uart_guest_map();

    int pa_bits = get_phys_bits(); /* max phys addr bits only support 48bits for now */
    uint64_t val = VTCR_RES1|VTCR_SH0_IS|VTCR_ORGN0_WBWA|VTCR_IRGN0_WBWA;
    val |= VTCR_TG0_4K;
    val |= VTCR_PS(4) | VTCR_T0SZ(64 - pa_bits);
    val |= VTCR_SL0(2); /* init lookup level = 0 */

    msr_sync(vtcr_el2, val);


    uint64_t vttbr_val = (uint64_t)stage2_L0 & (~0xFFFUL);
    vttbr_val |= (0ul<<48);

// #define INIT_ZERO_ADDR
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

    // dump_stage2_table(0);
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


#ifdef CONFIG_HOST_USE_2MB_MAPPING
    map_size = (map_size + MB(2) - 1) & (~(MB(2) - 1));
#else
    map_size = (map_size + PAGE_SIZE - 1) & PAGE_MASK;

#endif

    vmm_info("image size: 0x%x\n", map_size);

    asm volatile("msr sctlr_el1, xzr");

    uint32_t hcr_val = get_default_hcr_flags();
    //we only support AArch64 for now
    hcr_val |= (1 << 31);
    vmm_info("hcr: %x\n", hcr_val);
    msr(hcr_el2, hcr_val);

    size_t phy_memory_size = CONFIG_PHY_MEM_SIZE;

#ifdef CONFIG_HOST_USE_2MB_MAPPING
    build_hyper_two_level_page_table(MEM_VIRT_START, MEM_VIRT_START,
            MEM_VIRT_START + (uint64_t)map_size, MT_NORMAL);
#else
    build_hyper_three_level_page_table(MEM_VIRT_START, MEM_VIRT_START, map_size, MT_NORMAL);
#endif
    fix_map(0x09000000, 0x09000000, 0, MT_NORMAL);
    /* setup stage2 */
    enable_stage2_traslation(stage2_L0);

    // enable_mmu(stage2_direct_mapping_L0);
    enable_mmu(boot_pgtable);

}
