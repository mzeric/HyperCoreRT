#pragma once
#include "htypes.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t readl(void *addr);
void writel(void *addr, uint32_t value);

#ifdef __cplusplus
}
#endif