
#include "htypes.h"
#include "bitmap.h"
#include "compiler.h"
#include "vmio.h"
#include "string.h"

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))

#define SLOW_IMPL
#ifdef SLOW_IMPL
// 设置位
void __set_bit(uint64_t *bitmap, int n) {
    bitmap[n / BITS_PER_UINT64] |= (1UL << (n % BITS_PER_UINT64));
}

// 清除位
void __clear_bit(uint64_t *bitmap, int n) {
    bitmap[n / BITS_PER_UINT64] &= ~(1UL << (n % BITS_PER_UINT64));
}

void set_bits(uint64_t *bitmap, int start, int cnt) {
    // safe_printf("input map:%lx, s:%d nr:%d\n", bitmap[0], start, cnt);
    for (int i = start; i < start + cnt; ++i) {
        __set_bit(bitmap, i);
    }
        // safe_printf("output map:%lx\n", bitmap[0]);

}

void clear_bits(uint64_t *bitmap, int start, int cnt) {
    for (int i = start; i < start + cnt; ++i) {
        __clear_bit(bitmap, i);
    }
}
#else
void clear_bits(uint64_t *bitmap, int start, int cnt) {
    uint64_t mask = (~0ul << (start & (BITS_PER_LONG - 1)));
    uint64_t end = start + cnt;

    while (start + BITS_PER_LONG < end) {
        bitmap[start / BITS_PER_LONG] &= ~mask;
        start += BITS_PER_LONG;
        mask = ~0ul;
    }

    if (start < end) {
        mask = mask & ~(~0ul << (end & (BITS_PER_LONG - 1)));
        bitmap[start / BITS_PER_LONG] &= ~mask;
    }
}

void set_bits(uint64_t *bitmap, int start, int cnt) {


    uint64_t mask = (~0ul << (start & (BITS_PER_LONG - 1)));


    uint64_t end = start + cnt;

    start = round_down(start, BITS_PER_LONG);

    while (start + BITS_PER_LONG < end) {
        bitmap[start / BITS_PER_LONG] |= mask;
        start += BITS_PER_LONG;
        mask = ~0ul;
    }

    if (start < end) {
        mask = mask & ~(~0ul << (end & (BITS_PER_LONG - 1)));
        bitmap[start / BITS_PER_LONG] |= mask;
    }
}
#endif
int is_bit_set(uint64_t *bitmap, int n) {
    return bitmap[n / BITS_PER_UINT64] & (1UL << (n % BITS_PER_UINT64));
}

static uint64_t _find_next_bit(const uint64_t *addr, uint64_t nbits, uint64_t start,
                               uint64_t invert) {
    uint64_t tmp;

    if (unlikely(start >= nbits))
        return nbits;

    tmp = addr[start / BITS_PER_UINT64] ^ invert;

    /* the 1st may has offset. */
    tmp &= (~0ul << ((start) & (BITS_PER_LONG - 1)));
    start = round_down(start, BITS_PER_UINT64);

    while (!tmp) {
        start += BITS_PER_UINT64;
        if (start >= nbits)
            return nbits;

        tmp = addr[start / BITS_PER_UINT64] ^ invert;
    }

    return (start + __ffsl(tmp) < nbits ? start + __ffsl(tmp) : nbits);
}

uint64_t find_next_bit(const uint64_t *addr, uint64_t size, uint64_t offset) {
    return _find_next_bit(addr, size, offset, 0UL);
}

uint64_t find_next_zero_bit(uint64_t *map, uint64_t size, uint64_t start) {
    return _find_next_bit(map, size, start, ~0UL);
}

uint64_t bitmap_find_next_zero_area_off(uint64_t *map, uint64_t size, uint64_t start, uint32_t nr,
                                        uint64_t align_mask, uint64_t align_offset) {
    uint64_t index, end, i;

again:
    index = find_next_zero_bit(map, size, start);
    // safe_printf("debug: get start: %d\n", index);
    /* Align allocation */
    index = ALIGN_MASK(index + align_offset, align_mask) - align_offset;

    end = index + nr;
    if (end > size)
        return end;
    i = find_next_bit(map, end, index);
    if (i < end) {
        start = i + 1;
        goto again;
    }
    // safe_printf("debug: return start: %d\n", index);

    return index;
}

uint64_t bitmap_find_next_zero_area(uint64_t *map, uint64_t size, uint64_t start, uint32_t nr,
                                    uint64_t align_mask) {
    return bitmap_find_next_zero_area_off(map, size, start, nr, align_mask, 0);
}

void init_bitmap(uint64_t *bitmap, uint64_t len) { memset(bitmap, 0, len); }

