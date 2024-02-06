#pragma once

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

typedef struct {
    volatile long counter;
} atomic_t;

#define __INIT__ __attribute__((section))