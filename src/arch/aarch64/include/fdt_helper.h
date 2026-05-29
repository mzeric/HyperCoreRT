#pragma once

#include "libfdt_env.h"
#include <fdt.h>
#include <libfdt.h>
#include "libfdt_internal.h"

#include "htypes.h"

int fdt_get_reg_info(void *fdt, int node, uint64_t *addr, uint64_t *size);