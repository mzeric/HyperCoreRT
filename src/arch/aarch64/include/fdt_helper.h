#pragma once

#include "libfdt_env.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <fdt.h>
#include <libfdt.h>

#ifdef __cplusplus
}
#endif

/* libfdt_internal.h is pure C with implicit void* conversions.
 * Include it only from C translation units; C++ code that needs
 * FDT_RO_PROBE should include it via a thin C wrapper. */
#ifndef __cplusplus
#include "libfdt_internal.h"
#endif

#include "htypes.h"

int fdt_get_reg_info(void *fdt, int node, uint64_t *addr, uint64_t *size);