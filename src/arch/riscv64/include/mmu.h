#pragma once
#include "htypes.h"
#include "compiler.h"

typedef struct __packed { /* These are used in all kinds of entry. */
    u64 valid : 1;        /* Valid mapping */
    u64 read : 1;         /* == 1 in 4k map entries too */
    u64 write : 1;
    u64 exec : 1;

    u64 user : 1;
    u64 global : 1;
    u64 access : 1;
    u64 dirty : 1;

    u64 rsw : 2; // bits[8:9]

    u64 base : 44; /* Base address of block or next table */

    u64 res : 7; // bits[54:60]
    u64 pbmt : 2;
    u64 n : 1; // bits[63] 连续页表块
} page_table_walk_t;

typedef union {

    u64               bits;
    page_table_walk_t walk;
} ptw_t;

#define PAGE_ATTR_READ  (1u << 1)
#define PAGE_ATTR_WRITE (1u << 2)
#define PAGE_ATTR_EXEC  (1u << 3)
#define PAGE_ATTR_USER  (1u << 4)
#define PAGE_ATTR_DIRTY (1u << 7)

#define MEM_ACCESS_NONE  0x0
#define MEM_ACCESS_RWX   (PAGE_ATTR_EXEC | PAGE_ATTR_READ | PAGE_ATTR_WRITE)
#define MEM_ACCESS_RW    (PAGE_ATTR_READ | PAGE_ATTR_WRITE)

#define MB_2 (0x200000ul)

#define is_leaf(ptw) ((ptw)->walk.read || (ptw)->walk.write || (ptw)->walk.exec)

int pg_map(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, uint64_t map_size, int attr, int without_mmu);
int pg_map_huge(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, uint64_t map_size,
                int attr, int without_mmu);
paddr_t ptw_walk(ptw_t *cur_tbl, vaddr_t addr, int level);
int     enable_mmu(uint64_t pg_root);
int     pg_map_2MB(ptw_t *root, int start_level, vaddr_t vaddr, paddr_t paddr, int attr, int without_mmu);

int pg_map_stage2(vaddr_t vaddr, paddr_t paddr, uint64_t map_size, int attr, int without_mmu);

/* TLB flush helpers */
void hfence(void);