#pragma once
#include "vmio.h"
#include "mmu.h"
#include "lpae.h"
#include "arch_page.h"



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

#undef MB
#define MB(x) ((x##u) << 20)


void init_mm(void);

/* Saved MMU config for secondary CPU bring-up. */
extern uint64_t g_saved_tcr_el2;
extern void    *g_saved_ttbr0_el2;
void secondary_enable_mmu(void);

int          get_phys_bits();
void         print_addr_idx(vaddr_t addr);
paddr_t      __walk_page_table(lpae_t* cur_tbl_entry, vaddr_t addr, int level);


int   init_page_allocator();
void *alloc_mem_pool(uint64_t size);
void  free_mem_pool(void *ptr, uint64_t size);


void dump_kmalloc_status();
void page_summary();

void *ioremap(paddr_t phy, int size, int attr);
void  iounmap(vaddr_t vir, int size);
