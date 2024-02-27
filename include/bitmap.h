#pragma once
#include "htypes.h"

#define BITS_PER_BYTE   8
#define BITS_PER_UINT32 (sizeof(uint32_t) * BITS_PER_BYTE)
#define BITS_PER_UINT64 (sizeof(uint64_t) * BITS_PER_BYTE)
#define BITS_PER_LONG   BITS_PER_UINT64

void set_bits(uint64_t *bitmap, int start, int cnt);
void clear_bits(uint64_t *bitmap, int start, int cnt);
int  is_bit_set(uint64_t *bitmap, int n);

uint64_t find_next_bit(const uint64_t *addr, uint64_t size, uint64_t offset);
uint64_t find_next_zero_bit(uint64_t *map, uint64_t size, uint64_t start);
uint64_t bitmap_find_next_zero_area_off(uint64_t *map, uint64_t size, uint64_t start, uint32_t nr,
                                        uint64_t align_mask, uint64_t align_offset);
uint64_t bitmap_find_next_zero_area(uint64_t *map, uint64_t size, uint64_t start, uint32_t nr,
                                    uint64_t align_mask);