#pragma once
#include "compiler.h"
#include "htypes.h"
#include "kmalloc.h"

#ifdef __cplusplus
extern "C" {
#endif

void    *ioremap_page(paddr_t phy, int attr);
void     iounmap_page(vaddr_t vir);

#ifdef __cplusplus
}
#endif
