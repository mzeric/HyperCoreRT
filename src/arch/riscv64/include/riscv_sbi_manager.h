#pragma once

#include "htypes.h"
#include "arch_regs.h"

struct RiscvSbiResult {
    long error;
    u64 value;
};

class RiscvSbiManager final {
public:
    static bool IsExtensionSupported(u64 extension_id);
    static RiscvSbiResult NotSupported(void);
    static void HandleVsEcall(struct cpu_user_regs *args);

private:
    RiscvSbiManager() = delete;
};

void riscv_handle_vs_ecall(struct cpu_user_regs *args);
