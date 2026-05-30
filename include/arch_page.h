#pragma once
#include "compiler.h"
#include "htypes.h"
#include "spin_lock.h"

#ifndef MB
#define MB(x) ((x) << 20)
#endif


/*
    the 512G-1024G
    0x80_0000_0000 - 0xFF_0000_0000
    use pages_direct_mappint_L1
*/

#define KMAP_VIRT_START           (0xE0ul << 32)     /* 0xE0_0000_0000 */
#define KMAP_VIRT_END             (0xE01000ul << 16) /* 0xE0_1000_0000 256MB */
#define KMAP_PAGE_NUM             ((KMAP_VIRT_END - KMAP_VIRT_START) >> PAGE_SHIFT)
#define KMAP_L3_PAGE_NUM          (round_up(KMAP_PAGE_NUM, 512) / 512)
#define KMAP_L2_PAGE_NUM          (round_up(KMAP_L3_PAGE_NUM, 512) / 512)

#define DIRECT_MAPPING_VIRT_START   (0xF0UL << 32) /* 0xF0_0000_0000 */
#define DIRECT_MAPPING_VIRT_END     (0xFFUL << 32) /* 0xFF_0000_0000 max 16GB */
#define DIRECT_MAPPINT_PRIMARY_SIZE (1u << 30)

extern void *_hyper_end;
#define PAGE_PHYS_OFFSET ((((u64) &_hyper_end) + MB(1) + MB(2) - 1) & (~(MB(2) - 1)))
#define PAGE_VIRT_OFFSET DIRECT_MAPPING_VIRT_START

#define PAGE_VIRT_TO_PHYS(addr) (((u64)(addr)-PAGE_VIRT_OFFSET) + PAGE_PHYS_OFFSET)
#define PAGE_PHYS_TO_VIRT(addr) (((u64)(addr)-PAGE_PHYS_OFFSET) + PAGE_VIRT_OFFSET)
#define PAGE_VIR(pfn)     ((pfn) < 0 ? 0 : (PAGE_VIRT_OFFSET + ((pfn) << PAGE_SHIFT)))
#define PAGE_PHY(pfn)      ((pfn) < 0 ? 0 : (PAGE_PHYS_OFFSET + ((pfn) << PAGE_SHIFT)))

#define VIR_TO_FN(addr)                                                                            \
    ((u64)(addr) < PAGE_VIRT_OFFSET) ? -1 : (((u64)(addr)-PAGE_VIRT_OFFSET) >> PAGE_SHIFT)
#define PHY_TO_FN(addr)                                                                            \
    ((u64)(addr) < PAGE_PHYS_OFFSET) ? -1 : (((u64)(addr)-PAGE_PHYS_OFFSET) >> PAGE_SHIFT)


#define KMAP_TBL_PAGE_NUM                                                                          \
    ((KMAP_VIRT_END - KMAP_VIRT_START + ARM_PT_LEVEL_SIZE(1) - 1) >> ARM_PT_LEVEL_SHIFT(1))


int alloc_pages(int order);
int alloc_pages_cnt(int cnt);
int alloc_one_page();

void free_pages(int start, int order);
void free_pages_cnt(int pfn, int cnt);
void free_one_page(int pfn);

paddr_t vir_to_phy(vaddr_t v);
vaddr_t phy_to_vir(paddr_t v);

struct stage2_mm_info {
    int     pa_size;
    int     root_page_order;
    int     lookup_level;
    vaddr_t root_table;
};

int stage2_map(struct stage2_mm_info *info, vaddr_t vaddr, paddr_t paddr, uint64_t map_size,
               int attr, int acc);

int stage2_unmap(struct stage2_mm_info *info, vaddr_t vaddr, uint64_t map_size);

int host_map_one_page(vaddr_t vir, paddr_t phy, int attr);
// int host_map_pages(vaddr_t vir, paddr_t phy, int page_num, int attr);
void host_unmap_one_page(vaddr_t vir);
// void host_unmap_pages(vaddr_t vir, int size);

