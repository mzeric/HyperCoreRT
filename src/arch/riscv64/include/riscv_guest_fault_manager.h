#pragma once

#include "htypes.h"
#include "arch_regs.h"

class RiscvGuestFaultManager final {
public:
    static bool is_guest_trap(u64 hstatus);
    static int handle_exception(struct cpu_user_regs *regs, u64 cause);

private:
    RiscvGuestFaultManager() = delete;

    static int handle_stage2_fault(struct cpu_user_regs *regs, u64 cause);
    static int handle_virtual_instruction(struct cpu_user_regs *regs);
    static int redirect_guest_exception(struct cpu_user_regs *regs, u64 scause, u64 stval);
};
