#pragma once
#include "config.h"
#include "arch_page.h"

#define PAGE_SHIFT          CONFIG_PAGE_SHIFT
#define PAGE_SIZE           (1UL << PAGE_SHIFT)
#define PAGE_MASK           (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x)       (((x) + PAGE_SIZE - 1) & PAGE_MASK)

#define PAGE_LEVEL_MAX      (3)
#define PAGE_LEVEL_WIDTH    (9u)
#define PTRS_PER_ENTRY      (PAGE_SIZE/sizeof(u64))
#define PAGE_LEVEL_SHIFT(v) (PAGE_SHIFT + PAGE_LEVEL_WIDTH * (PAGE_LEVEL_MAX - (v)))



void init_mm();
