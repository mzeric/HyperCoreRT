#pragma once

#ifndef __ASSEMBLY__

#include <inttypes.h>
#include "stdbool.h"
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t  u8;
typedef uint64_t size_t;

typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;

typedef struct {
    volatile long counter;
} atomic_t;


#ifndef NULL
#define NULL 0
#endif


#define min(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x < _y ? _x : _y; })

#define max(x,y) ({ \
        const typeof(x) _x = (x);       \
        const typeof(y) _y = (y);       \
        (void) (&_x == &_y);            \
        _x > _y ? _x : _y; })
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define container_of(ptr, type, member) ({                      \
        typeof( ((type *)0)->member ) *__mptr = (ptr);          \
        (type *)( (char *)__mptr - offsetof(type,member) );})


#define BITS_PER_LONG (64)
#define GENMASK(h, l) \
    (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))


#endif