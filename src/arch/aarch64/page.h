#pragma once
#include "autoconf.h"
#include "compiler.h"
#include "htypes.h"
/*
    refs to xen:
        * arch/arm/include/asm/lpae.h
        * arch/arm/include/asm/page.h

*/

/* Shareability values for the LPAE entries */
#define LPAE_SH_NON_SHAREABLE 0x0
#define LPAE_SH_UNPREDICTALE  0x1
#define LPAE_SH_OUTER         0x2
#define LPAE_SH_INNER         0x3

/*
 * Attribute Indexes.
 *
 * These are valid in the AttrIndx[2:0] field of an LPAE stage 1 page
 * table entry. They are indexes into the bytes of the MAIR*
 * registers, as defined below.
 *
 */
#define MT_DEVICE_nGnRnE 0x0
#define MT_NORMAL_NC     0x1
#define MT_NORMAL_WT     0x2
#define MT_NORMAL_WB     0x3
#define MT_DEVICE_nGnRE  0x4
#define MT_NORMAL        0x7

/*
 * LPAE Memory region attributes. Indexed by the AttrIndex bits of a
 * LPAE entry; the 8-bit fields are packed little-endian into MAIR0 and MAIR1.
 *
 * See section "Device memory" B2.7.2 in ARM DDI 0487B.a for more
 * details about the meaning of *G*R*E.
 *
 *                    ai    encoding
 *   MT_DEVICE_nGnRnE 000   0000 0000  -- Strongly Ordered/Device nGnRnE
 *   MT_NORMAL_NC     001   0100 0100  -- Non-Cacheable
 *   MT_NORMAL_WT     010   1010 1010  -- Write-through
 *   MT_NORMAL_WB     011   1110 1110  -- Write-back
 *   MT_DEVICE_nGnRE  100   0000 0100  -- Device nGnRE
 *   ??               101
 *   reserved         110
 *   MT_NORMAL        111   1111 1111  -- Write-back write-allocate
 *
 * /!\ It is not possible to combine the definition in MAIRVAL and then
 * split because it would result to a 64-bit value that some assembler
 * doesn't understand.
 */
#ifdef __ASSEMBLY__
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

#define BIT(pos, sfx)   (_AC(1, sfx) << (pos))

#define _MAIR0(attr, mt) (_AC(attr, ULL) << ((mt) * 8))
#define _MAIR1(attr, mt) (_AC(attr, ULL) << (((mt) * 8) - 32))

#define MAIR0VAL (_MAIR0(0x00, MT_DEVICE_nGnRnE)| \
                  _MAIR0(0x44, MT_NORMAL_NC)    | \
                  _MAIR0(0xaa, MT_NORMAL_WT)    | \
                  _MAIR0(0xee, MT_NORMAL_WB))

#define MAIR1VAL (_MAIR1(0x04, MT_DEVICE_nGnRE) | \
                  _MAIR1(0xff, MT_NORMAL))

#define MAIRVAL (MAIR1VAL << 32 | MAIR0VAL)

#define PAGE_SHIFT CONFIG_PAGE_SHIFT
#define PADDR_BITS CONFIG_PADDR_BITS
#define MEM_VIRT_START CONFIG_ENTRY_ADDR

#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_MASK           (~(PAGE_SIZE-1))
#define PAGE_OFFSET(ptr)    ((unsigned long)(ptr) & ~PAGE_MASK)
#define PAGE_ALIGN(x)       (((x) + PAGE_SIZE - 1) & PAGE_MASK)

#define PADDR_MASK          ((_AC(1,ULL) << PADDR_BITS) - 1)
#define VADDR_MASK          (~_AC(0,UL) >> (BITS_PER_LONG - VADDR_BITS))

#ifndef __ASSEMBLY__
typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;





/******************************************************************************
 * ARMv7-A LPAE pagetables: 3-level trie, mapping 40-bit input to
 * 40-bit output addresses.  Tables at all levels have 512 64-bit entries
 * (i.e. are 4Kb long).
 *
 * The bit-shuffling that has the permission bits in branch nodes in a
 * different place from those in leaf nodes seems to be to allow linear
 * pagetable tricks.  If we're not doing that then the set of permission
 * bits that's not in use in a given node type can be used as
 * extra software-defined bits.
 */

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
/* Permission mask: xn, write, read */
#define P2M_PERM_MASK (0x00400000000000C0ULL)
#define P2M_CLEAR_PERM(pte) ((pte).bits & ~P2M_PERM_MASK)

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

typedef uint64_t mfn_t;
static inline mfn_t _mfn(uint64_t n) { return n; }
static inline uint64_t mfn_x(mfn_t n) { return n; }

static inline int lpae_is_valid(lpae_t pte)
{
    return pte.walk.valid;
}

/*
 * lpae_is_* don't check the valid bit. This gives an opportunity for the
 * callers to operate on the entry even if they are not valid. For
 * instance to store information in advance.
 */
static inline int lpae_is_table(lpae_t pte, unsigned int level)
{
    return (level < 3) && pte.walk.table;
}

static inline int lpae_is_mapping(lpae_t pte, unsigned int level)
{
    if ( level == 3 )
        return pte.walk.table;
    else
        return !pte.walk.table;
}

static inline int lpae_is_superpage(lpae_t pte, unsigned int level)
{
    return (level < 3) && lpae_is_mapping(pte, level);
}

#define lpae_get_mfn(pte)    (_mfn((pte).walk.base))
#define lpae_set_mfn(pte, mfn)  ((pte).walk.base = mfn_x(mfn))

/* Generate an array @var containing the offset for each level from @addr */
#define DECLARE_OFFSETS(var, addr)          \
    const unsigned int var[4] = {           \
        zeroeth_table_offset(addr),         \
        first_table_offset(addr),           \
        second_table_offset(addr),          \
        third_table_offset(addr)            \
    }

#endif


/*
 * AArch64 supports pages with different sizes (4K, 16K, and 64K).
 * Provide a set of generic helpers that will compute various
 * information based on the page granularity.
 *
 * Note the parameter 'gs' is the page shift of the granularity used.
 * Some macro will evaluate 'gs' twice rather than storing in a
 * variable. This is to allow using the macros in assembly.
 */

/*
 * Granularity | PAGE_SHIFT | LPAE_SHIFT
 * -------------------------------------
 * 4K          | 12         | 9
 * 16K         | 14         | 11
 * 64K         | 16         | 13
 *
 * This is equivalent to LPAE_SHIFT = PAGE_SHIFT - 3
 */
#define LPAE_SHIFT_GS(gs)         ((gs) - 3)
#define LPAE_ENTRIES_GS(gs)       (_AC(1, U) << LPAE_SHIFT_GS(gs))
#define LPAE_ENTRY_MASK_GS(gs)    (LPAE_ENTRIES_GS(gs) - 1)

#define LEVEL_ORDER_GS(gs, lvl)   ((3 - (lvl)) * LPAE_SHIFT_GS(gs))
#define LEVEL_SHIFT_GS(gs, lvl)   (LEVEL_ORDER_GS(gs, lvl) + (gs))
#define LEVEL_SIZE_GS(gs, lvl)    (_AT(paddr_t, 1) << LEVEL_SHIFT_GS(gs, lvl))

/* Offset in the table at level 'lvl' */
#define LPAE_TABLE_INDEX_GS(gs, lvl, addr)   \
    (((addr) >> LEVEL_SHIFT_GS(gs, lvl)) & LPAE_ENTRY_MASK_GS(gs))

/*
 * These numbers add up to a 48-bit input address space.
 *
 * On 32-bit the zeroeth level does not exist, therefore the total is
 * 39-bits. The ARMv7-A architecture actually specifies a 40-bit input
 * address space for the p2m, with an 8K (1024-entry) top-level table.
 * However Xen only supports 16GB of RAM on 32-bit ARM systems and
 * therefore 39-bits are sufficient.
 */

#define ARM_PT_LPAE_SHIFT         LPAE_SHIFT_GS(PAGE_SHIFT)
#define ARM_PT_LPAE_ENTRIES       LPAE_ENTRIES_GS(PAGE_SHIFT)
#define ARM_PT_LPAE_ENTRY_MASK    LPAE_ENTRY_MASK_GS(PAGE_SHIFT)

#define ARM_PT_LEVEL_SHIFT(lvl)   LEVEL_SHIFT_GS(PAGE_SHIFT, lvl)
#define ARM_PT_LEVEL_ORDER(lvl)   LEVEL_ORDER_GS(PAGE_SHIFT, lvl)
#define ARM_PT_LEVEL_SIZE(lvl)    LEVEL_SIZE_GS(PAGE_SHIFT, lvl)
#define ARM_PT_LEVEL_MASK(lvl)    (~(ARM_PT_LEVEL_SIZE(lvl) - 1))

/* Convenience aliases */
#define THIRD_SHIFT         ARM_PT_LEVEL_SHIFT(3)
#define THIRD_ORDER         ARM_PT_LEVEL_ORDER(3)
#define THIRD_SIZE          ARM_PT_LEVEL_SIZE(3)
#define THIRD_MASK          ARM_PT_LEVEL_MASK(3)

#define SECOND_SHIFT        ARM_PT_LEVEL_SHIFT(2)
#define SECOND_ORDER        ARM_PT_LEVEL_ORDER(2)
#define SECOND_SIZE         ARM_PT_LEVEL_SIZE(2)
#define SECOND_MASK         ARM_PT_LEVEL_MASK(2)

#define FIRST_SHIFT         ARM_PT_LEVEL_SHIFT(1)
#define FIRST_ORDER         ARM_PT_LEVEL_ORDER(1)
#define FIRST_SIZE          ARM_PT_LEVEL_SIZE(1)
#define FIRST_MASK          ARM_PT_LEVEL_MASK(1)

#define ZEROETH_SHIFT       ARM_PT_LEVEL_SHIFT(0)
#define ZEROETH_ORDER       ARM_PT_LEVEL_ORDER(0)
#define ZEROETH_SIZE        ARM_PT_LEVEL_SIZE(0)
#define ZEROETH_MASK        ARM_PT_LEVEL_MASK(0)

/* Calculate the offsets into the pagetables for a given VA */
#define zeroeth_linear_offset(va) ((va) >> ZEROETH_SHIFT)
#define first_linear_offset(va) ((va) >> FIRST_SHIFT)
#define second_linear_offset(va) ((va) >> SECOND_SHIFT)
#define third_linear_offset(va) ((va) >> THIRD_SHIFT)

#define TABLE_OFFSET(offs) (_AT(unsigned int, offs) & ARM_PT_LPAE_ENTRY_MASK)
#define first_table_offset(va)  TABLE_OFFSET(first_linear_offset(va))
#define second_table_offset(va) TABLE_OFFSET(second_linear_offset(va))
#define third_table_offset(va)  TABLE_OFFSET(third_linear_offset(va))
#ifdef CONFIG_PHYS_ADDR_T_32
#define zeroeth_table_offset(va)  0
#else
#define zeroeth_table_offset(va)  TABLE_OFFSET(zeroeth_linear_offset(va))
#endif

/*
 * Macros to define page-tables:
 *  - DEFINE_BOOT_PAGE_TABLE{,S} are used to define one or multiple
 *  page-table that are used in assembly code before BSS is zeroed.
 *  - DEFINE_PAGE_TABLE{,S} are used to define one or multiple
 *  page-tables to be used after BSS is zeroed (typically they are only used
 *  in C).
 */
#define DEFINE_BOOT_PAGE_TABLES(name, nr)                                     \
lpae_t __aligned(PAGE_SIZE) __section(".data.page_aligned")                   \
    name[ARM_PT_LPAE_ENTRIES * (nr)]

#define DEFINE_BOOT_PAGE_TABLE(name) DEFINE_BOOT_PAGE_TABLES(name, 1)

#define DEFINE_PAGE_TABLES(name, nr)                    \
lpae_t __aligned(PAGE_SIZE) name[ARM_PT_LPAE_ENTRIES * (nr)]

#define DEFINE_PAGE_TABLE(name) DEFINE_PAGE_TABLES(name, 1)
