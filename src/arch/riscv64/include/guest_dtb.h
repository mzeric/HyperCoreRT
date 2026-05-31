#pragma once
#include "htypes.h"

#define RISCV_DEFAULT_GUEST_ENTRY    0x90080000UL
#define RISCV_DEFAULT_GUEST_DTB_ADDR 0x90f00000UL
#define RISCV_DEFAULT_GUEST_RAM_BASE 0x90000000UL
#define RISCV_DEFAULT_GUEST_RAM_SIZE 0x01000000UL

void riscv_guest_config_init(void *host_fdt);
void riscv_guest_dtb_init(void *host_fdt);
u64  riscv_guest_entry(void);
u64  riscv_guest_dtb_addr(void);
u64  riscv_guest_ram_base(void);
u64  riscv_guest_ram_size(void);
u32  riscv_guest_vcpu_count(void);
