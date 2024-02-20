#pragma once
#include "compiler.h"

#define vmm_debug(fmt, arg...)                                                 \
    do {                                                                       \
        printf("[Debug][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);           \
    } while (0)

#define vmm_info(fmt, arg...)                                                  \
    do {                                                                       \
        printf("[Info][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);            \
    } while (0)

#define vmm_warn(fmt, arg...)                                                  \
    do {                                                                       \
        printf("[Warn][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);            \
    } while (0)

#define vmm_err(fmt, arg...)                                                   \
    do {                                                                       \
        printf("[Error][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);           \
    } while (0)

#define vmm_fatal(fmt, arg...)                                                                     \
    do {                                                                                           \
        printf("[Fatal][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);                               \
        panic(".....\n");                                                                          \
    } while (0)

#define vmm_printf(fmt, arg...)                                                                    \
    do {                                                                                           \
        printf(fmt, ##arg);                                                                        \
    } while (0)

typedef struct {
    volatile long counter;
} atomic_t;

#define WARN_ON(p)                                                             \
    do {                                                                       \
        int real = (p);                                                        \
        if (unlikely(real))                                                    \
            vmm_warn(stringify(p));                                                    \
    } while (0)

#define vmm_exit(r) exit((r))
#define __INIT__ __attribute__((section))