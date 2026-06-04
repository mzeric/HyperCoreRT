#pragma once

#include "htypes.h"
#include "arch_regs.h"

struct RiscvSbiResult {
    long error;
    u64 value;
};

bool riscv_sbi_extension_supported(u64 extension_id);
struct RiscvSbiResult riscv_sbi_not_supported(void);
void riscv_sbi_handle_vs_ecall(struct cpu_user_regs *args);
void riscv_handle_vs_ecall(struct cpu_user_regs *args);
