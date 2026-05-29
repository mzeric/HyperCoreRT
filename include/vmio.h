#pragma once
#include "compiler.h"
#include "htypes.h"
#include "safe_printf.h"
#include <stdio.h>

void panic(char *msg);

#define hyper_debug(fmt, arg...)                                                 \
    do {                                                                       \
        printf("[Debug][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);           \
    } while (0)

#define hyper_info(fmt, arg...)                                                  \
    do {                                                                       \
        printf("[Info][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);            \
    } while (0)

#define hyper_warn(fmt, arg...)                                                  \
    do {                                                                       \
        printf("[Warn][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);            \
    } while (0)

#define hyper_err(fmt, arg...)                                                   \
    do {                                                                       \
        printf("[Error][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);           \
    } while (0)

#define hyper_fatal(fmt, arg...)                                                                     \
    do {                                                                                           \
        printf("[Fatal][%s:%d]" fmt, __FUNCTION__, __LINE__, ##arg);                               \
        panic(".....\n");                                                                          \
    } while (0)

#define hyper_printf(fmt, arg...)                                                                    \
    do {                                                                                           \
        printf(fmt, ##arg);                                                                        \
    } while (0)


#define WARN_ON(p)                                                                                 \
    do {                                                                                           \
        int real = (p);                                                                            \
        if (unlikely(real))                                                                        \
            hyper_warn(stringify(p));                                                                \
    } while (0)

#define BUG_ON(p)                                                                                  \
    do {                                                                                           \
        int real = (p);                                                                            \
        if (unlikely(real))                                                                        \
            hyper_fatal(stringify(p));                                                               \
    } while (0)


#define ASSERT(p)   BUG_ON(!(p))
#define hyper_exit(r) do { while(1); } while(0)
#define __INIT__    __attribute__((section))

#undef printf