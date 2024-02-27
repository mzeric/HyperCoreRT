#include "vmmio.h"
#include "io.h"


uint32_t readl(void *addr) { return *(volatile uint32_t *)addr; }

void writel(void *addr, uint32_t value) { *(volatile uint32_t *)addr = value; }
