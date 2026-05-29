#pragma once
#include "htypes.h"
#include "compiler.h"
#include "arch_page.h"

typedef struct __packed {
    /* These are used in all kinds of entry. */
    unsigned long valid:1;      /* Valid mapping */
    unsigned long table:1;      /* == 1 in 4k map entries too */

    /*
     * These ten bits are only used in Block entries and are ignored
     * in Table entries.
     */
    unsigned long ai:3;         /* Attribute Index */
    unsigned long ns:1;         /* Not-Secure */
    unsigned long up:1;         /* Unpriviledged access */
    unsigned long ro:1;         /* Read-Only */
    unsigned long sh:2;         /* Shareability */
    unsigned long af:1;         /* Access Flag */
    unsigned long ng:1;         /* Not-Global */

    /* The base address must be appropriately aligned for Block entries */
    unsigned long long base:36; /* Base address of block or next table */
    unsigned long sbz:4;        /* Must be zero */

    /*
     * These seven bits are only used in Block entries and are ignored
     * in Table entries.
     */
    unsigned long contig:1;     /* In a block of 16 contiguous entries */
    unsigned long pxn:1;        /* Privileged-XN */
    unsigned long xn:1;         /* eXecute-Never */
    unsigned long avail:4;      /* Ignored by hardware */

    /*
     * These 5 bits are only used in Table entries and are ignored in
     * Block entries.
     */
    unsigned long pxnt:1;       /* Privileged-XN */
    unsigned long xnt:1;        /* eXecute-Never */
    unsigned long apt:2;        /* Access Permissions */
    unsigned long nst:1;        /* Not-Secure */
} lpae_pt_t;

/*
 * The p2m tables have almost the same layout, but some of the permission
 * and cache-control bits are laid out differently (or missing).
 */
typedef struct __packed {
    /* These are used in all kinds of entry. */
    unsigned long valid:1;      /* Valid mapping */
    unsigned long table:1;      /* == 1 in 4k map entries too */

    /*
     * These ten bits are only used in Block entries and are ignored
     * in Table entries.
     */
    unsigned long mattr:4;      /* Memory Attributes */
    unsigned long read:1;       /* Read access */
    unsigned long write:1;      /* Write access */
    unsigned long sh:2;         /* Shareability */
    unsigned long af:1;         /* Access Flag */
    unsigned long sbz4:1;

    /* The base address must be appropriately aligned for Block entries */
    unsigned long long base:36; /* Base address of block or next table */
    unsigned long sbz3:4;

    /*
     * These seven bits are only used in Block entries and are ignored
     * in Table entries.
     */
    unsigned long contig:1;     /* In a block of 16 contiguous entries */
    unsigned long sbz2:1;
    unsigned long xn:1;         /* eXecute-Never */
    unsigned long type:4;       /* Ignore by hardware. Used to store p2m types */

    unsigned long sbz1:5;
} lpae_p2m_t;

typedef struct __packed{
    /* These are used in all kinds of entry. */
    unsigned long valid:1;      /* Valid mapping */
    unsigned long table:1;      /* == 1 in 4k map entries too */

    /*
     * These ten bits are only used in Block entries and are ignored
     * in Table entries.
     */
    unsigned long mattr:4;      /* Memory Attributes */
    unsigned long read:1;       /* Read access */
    unsigned long write:1;      /* Write access */
    unsigned long sh:2;         /* Shareability */
    unsigned long af:1;         /* Access Flag */
    unsigned long sbz4:1;

    /* The base address must be appropriately aligned for Block entries */
    unsigned long long base:36; /* Base address of block or next table */
    unsigned long sbz3:4;

    /*
     * These seven bits are only used in Block entries and are ignored
     * in Table entries.
     */
    unsigned long contig:1;     /* In a block of 16 contiguous entries */
    unsigned long sbz2:1;
    unsigned long xn:1;         /* eXecute-Never */
    unsigned long type:4;       /* Ignore by hardware. Used to store p2m types */

    unsigned long sbz1:5;
}lpae_block_t;

/*
 * Stage-2 memory type — drives the page-table MAIR index and the
 * Inner/Outer Shareability bits.  Only the two flavours that the
 * current hypervisor actually maps are listed; add more here if you
 * grow the set (do *not* re-introduce a Xen-style giant enum).
 */
enum mem_type {
    MEM_NORMAL_RW = 0,  /* WB cacheable, inner-shareable (guest RAM)        */
    MEM_DEVICE_NC,      /* Device-nGnRE, outer-shareable (MMIO passthrough) */
    MEM_DEVICE,         /* Device-nGnRnE (strongest ordering, rarely used)  */
};

/*
 * Per-entry access overlay applied on top of mem_type.  RWX is the
 * "all permissions" default; the other two cover the only narrowings
 * the current code needs (MMIO no-exec, and AF=0 for lazy / fault-in
 * mappings).
 */
enum mem_access {
    MEM_ACCESS_RWX = 0, /* full read+write+execute                          */
    MEM_ACCESS_RW,      /* exec-never (data + MMIO passthrough)             */
    MEM_ACCESS_NONE,    /* AF cleared: first touch faults to EL2            */
};


/*
 * Walk is the common bits of p2m and pt entries which are needed to
 * simply walk the table (e.g. for debug).
 */
typedef struct __packed {
    /* These are used in all kinds of entry. */
    unsigned long valid:1;      /* Valid mapping */
    unsigned long table:1;      /* == 1 in 4k map entries too */

    unsigned long pad2:10;

    /* The base address must be appropriately aligned for Block entries */
    unsigned long long base:36; /* Base address of block or next table */

    unsigned long pad1:16;
} lpae_walk_t;

typedef union {
    uint64_t bits;
    lpae_pt_t pt;
    lpae_p2m_t p2m;
    lpae_walk_t walk;
} lpae_t;

int get_pa_bits();

int s2_setup_info(struct stage2_mm_info *info, int pa_regs);
int s2_alloc_root_pages(struct stage2_mm_info *info);


void build_hyper_table(lpae_t *table_current_level, vaddr_t next_tbl, vaddr_t virt, uint8_t level, int attr);

int __build_hyp_two_level_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t mem_size,
        int attr, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2);

int __build_hyp_three_level_page_table(vaddr_t virt_start, paddr_t phys_start, uint64_t map_size,
        int attr, lpae_t* table_L0, lpae_t* table_L1, lpae_t* table_L2,
        lpae_t* table_L3);

int build_stage2_page_table(vaddr_t virt_start, paddr_t phys_start,
        uint64_t map_size, lpae_t* L0, lpae_t* L1, lpae_t* L2, lpae_t* L3,
        int attr);

int __ptw_map_4k_page(vaddr_t vir_addr, paddr_t phy_addr, lpae_t *cur_tbl, int level,
                      int attr);
int __ptw_unmap_4k_page(vaddr_t vir_addr, lpae_t *pre_tbl, int level);


void build_s2_table(lpae_t *cur_table, vaddr_t next_table, vaddr_t virt, uint8_t level, int attr);
int  stage2_map_4k(lpae_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, int attr, int acc);


uint64_t pte_offset(vaddr_t virt, uint8_t level);

/* EL2 stage-1 entry. */
lpae_t make_stage1_entry(paddr_t pa, unsigned int attr_index);

/* Guest stage-2 entry for a given physical address. */
lpae_t make_stage2_entry(paddr_t pa, enum mem_type type, enum mem_access access);