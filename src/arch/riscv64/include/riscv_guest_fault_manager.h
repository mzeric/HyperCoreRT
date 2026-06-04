#pragma once

#include "htypes.h"
#include "arch_regs.h"

bool riscv_guest_fault_is_guest_trap(u64 hstatus);
int riscv_guest_fault_handle_exception(struct cpu_user_regs *regs, u64 cause);
