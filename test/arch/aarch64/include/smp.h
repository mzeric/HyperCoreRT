#pragma once

#include "config.h"
#include "inline_asm.h"
#include <stdint.h>

/* Test stub — single CPU, cpu_id() always returns 0 */
static inline int cpu_id(void)
{
    return 0;
}
