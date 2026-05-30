#pragma once
#include "compiler.h"
#include "htypes.h"
#include "safe_printf.h"
#include "config.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

void panic(const char *msg);

/* Runtime log level — defined in log.c. */
extern int g_log_level;

/* Internal helper: compile-time + runtime dual-gate. */
#define _hyper_log(level, tag, fmt, ...)                                 \
    do {                                                                 \
        if ((level) < LOG_LEVEL) break;          /* compile-time gate */ \
        if ((level) < g_log_level) break;        /* runtime gate */      \
        log_lock();                                                      \
        safe_printf("[" tag "][%s:%d]" fmt "\n",                        \
                    __FUNCTION__, __LINE__, ##__VA_ARGS__);              \
        log_unlock();                                                    \
    } while (0)

#define hyper_debug(fmt, ...) _hyper_log(LOG_LEVEL_DEBUG, "Debug", fmt, ##__VA_ARGS__)
#define hyper_info(fmt, ...)  _hyper_log(LOG_LEVEL_INFO,  "Info",  fmt, ##__VA_ARGS__)
#define hyper_warn(fmt, ...)  _hyper_log(LOG_LEVEL_WARN,  "Warn",  fmt, ##__VA_ARGS__)
#define hyper_err(fmt, ...)   _hyper_log(LOG_LEVEL_ERR,   "Error", fmt, ##__VA_ARGS__)
#define hyper_fatal(fmt, ...)                                           \
    do {                                                                \
        _hyper_log(LOG_LEVEL_FATAL, "Fatal", fmt, ##__VA_ARGS__);      \
        panic(".....\n");                                               \
    } while (0)

#define hyper_printf(fmt, ...) safe_printf(fmt, ##__VA_ARGS__)


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

#ifdef __cplusplus
}
#endif