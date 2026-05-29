#pragma once
#include "compiler.h"
#include "htypes.h"
#include "kmalloc.h"

void    *ioremap_page(paddr_t phy, int attr);
void     iounmap_page(vaddr_t vir);
