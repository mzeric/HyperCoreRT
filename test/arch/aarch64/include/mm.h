#pragma once
#include "vmio.h"
#include "mmu.h"
#include "lpae.h"

#define pfn_to_paddr(pfn) ((paddr_t)(pfn) << PAGE_SHIFT)
#define paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))
#define paddr_to_pdx(pa)  mfn_to_pdx(maddr_to_mfn(pa))
#define gfn_to_gaddr(gfn) pfn_to_paddr(gfn_x(gfn))
#define gaddr_to_gfn(ga)  _gfn(paddr_to_pfn(ga))
#define mfn_to_maddr(mfn) pfn_to_paddr(mfn_x(mfn))
#define maddr_to_mfn(ma)  _mfn(paddr_to_pfn(ma))
#define vmap_to_mfn(va)   maddr_to_mfn(virt_to_maddr((vaddr_t)va))
#define vmap_to_page(va)  mfn_to_page(vmap_to_mfn(va))


extern void *_hyper_start, *_hyper_end;

/*
 * Stage 2 Memory Type.
 *
 * These are valid in the MemAttr[3:0] field of an LPAE stage 2 page
 * table entry.
 *
 */
#define MATTR_DEV     0x1
#define MATTR_MEM_NC  0x5
#define MATTR_MEM     0xf

#define MB(x) ((x##u) << 20)

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

#define DIRECT_MAPPING_VIRT_START (0xF0UL << 32)     /* 0xF0_0000_0000 */
#define DIRECT_MAPPING_VIRT_END   (0xFFUL << 32)     /* 0xFF_0000_0000 max 16GB */



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


void init_mm(void);

paddr_t vir_to_phy(vaddr_t v);
vaddr_t phy_to_vir(paddr_t v);

int          get_phys_bits();
void         print_addr_idx(vaddr_t addr);
paddr_t      __walk_page_table(lpae_t* cur_tbl_entry, vaddr_t addr, int level);

int alloc_pages(int order);
int alloc_pages_cnt(int cnt);
int alloc_one_page();

void free_pages(int start, int order);
void free_pages_cnt(int pfn, int cnt);
void free_one_page(int pfn);

int   init_page_allocator();
void *alloc_mem_pool(uint64_t size);
void  free_mem_pool(void *ptr, uint64_t size);

void *kmalloc(uint64_t size);
void *kfree(void *ptr);

int      init_kmap();
int init_kmalloc();

uint64_t __vmalloc(size_t size);
void     __vfree(uint64_t ptr, size_t size);
void    *ioremap_page(paddr_t phy, int attr);
void     iounmap_page(vaddr_t vir);
int      host_map_one_page(vaddr_t vir, paddr_t phy, int attr);
void dump_kmalloc_status();
void page_summary();

void *ioremap(paddr_t phy, int size, int attr);
void  iounmap(vaddr_t vir, int size);
