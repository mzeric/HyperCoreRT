#pragma once
#include "vmmio.h"

#define pfn_to_paddr(pfn) ((paddr_t)(pfn) << PAGE_SHIFT)
#define paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))
#define paddr_to_pdx(pa)  mfn_to_pdx(maddr_to_mfn(pa))
#define gfn_to_gaddr(gfn) pfn_to_paddr(gfn_x(gfn))
#define gaddr_to_gfn(ga)  _gfn(paddr_to_pfn(ga))
#define mfn_to_maddr(mfn) pfn_to_paddr(mfn_x(mfn))
#define maddr_to_mfn(ma)  _mfn(paddr_to_pfn(ma))
#define vmap_to_mfn(va)   maddr_to_mfn(virt_to_maddr((vaddr_t)va))
#define vmap_to_page(va)  mfn_to_page(vmap_to_mfn(va))


/*
 * Additional access types, which are used to further restrict
 * the permissions given my the p2m_type_t memory type.  Violations
 * caused by p2m_access_t restrictions are sent to the vm_event
 * interface.
 *
 * The access permissions are soft state: when any ambiguous change of page
 * type or use occurs, or when pages are flushed, swapped, or at any other
 * convenient type, the access permissions can get reset to the p2m_domain
 * default.
 */
typedef enum {
    /* Code uses bottom three bits with bitmask semantics */
    p2m_access_n     = 0, /* No access allowed. */
    p2m_access_r     = 1 << 0,
    p2m_access_w     = 1 << 1,
    p2m_access_x     = 1 << 2,
    p2m_access_rw    = p2m_access_r | p2m_access_w,
    p2m_access_rx    = p2m_access_r | p2m_access_x,
    p2m_access_wx    = p2m_access_w | p2m_access_x,
    p2m_access_rwx   = p2m_access_r | p2m_access_w | p2m_access_x,

    p2m_access_rx2rw = 8, /* Special: page goes from RX to RW on write */
    p2m_access_n2rwx = 9, /* Special: page goes from N to RWX on access, *
                           * generates an event but does not pause the
                           * vcpu */

    /* NOTE: Assumed to be only 4 bits right now on x86. */
} p2m_access_t;


/*
 * List of possible type for each page in the p2m entry.
 * The number of available bit per page in the pte for this purpose is 4 bits.
 * So it's possible to only have 16 fields. If we run out of value in the
 * future, it's possible to use higher value for pseudo-type and don't store
 * them in the p2m entry.
 */
typedef enum {
    p2m_invalid = 0,    /* Nothing mapped here */
    p2m_ram_rw,         /* Normal read/write guest RAM */
    p2m_ram_ro,         /* Read-only; writes are silently dropped */
    p2m_mmio_direct_dev,/* Read/write mapping of genuine Device MMIO area */
    p2m_mmio_direct_nc, /* Read/write mapping of genuine MMIO area non-cacheable */
    p2m_mmio_direct_c,  /* Read/write mapping of genuine MMIO area cacheable */
    p2m_map_foreign_rw, /* Read/write RAM pages from foreign domain */
    p2m_map_foreign_ro, /* Read-only RAM pages from foreign domain */
    p2m_grant_map_rw,   /* Read/write grant mapping */
    p2m_grant_map_ro,   /* Read-only grant mapping */
    /* The types below are only used to decide the page attribute in the P2M */
    p2m_iommu_map_rw,   /* Read/write iommu mapping */
    p2m_iommu_map_ro,   /* Read-only iommu mapping */
    p2m_max_real_type,  /* Types after this won't be store in the p2m */
} p2m_type_t;

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

void build_hyper_table(lpae_t *table_current_level, vaddr_t next_tbl, vaddr_t virt, uint8_t level, int attr);

int __build_hyper_two_level_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t mem_size,
        int attr, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2);

int __build_hyper_three_level_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t map_size,
        int attr, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2,
        lpae_t* table_L3);

int build_stage2_page_table(vaddr_t virt_start, paddr_t phys_start,
        uint64_t map_size, lpae_t* L0, lpae_t* L1, lpae_t* L2, lpae_t* L3,
        int attr);

lpae_t make_p2m_table_entry(vaddr_t virt, int attr);
lpae_t make_lpae_entry(mfn_t mfn, unsigned int attr);
void init_mm(void);

paddr_t vir_to_phy(vaddr_t v);
int get_phys_bits();