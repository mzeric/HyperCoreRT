#pragma once
#include "htypes.h"

#define RISCV_GUEST_DTB_ADDR 0x90f00000UL

void riscv_guest_dtb_init(void *host_fdt);
u64  riscv_guest_dtb_addr(void);
