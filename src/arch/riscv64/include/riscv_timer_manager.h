#pragma once

#include "htypes.h"
#include "vcpu.h"

class RiscvTimerManager final {
public:
    static void Arm(vcpu_t *vcpu, u64 deadline_cycles);
    static void Clear(vcpu_t *vcpu);
    static void Refresh(vcpu_t *vcpu);

private:
    RiscvTimerManager() = delete;
};

void riscv_vcpu_timer_arm_current(u64 deadline_cycles);
void riscv_vcpu_timer_clear_current(void);
void riscv_vcpu_timer_refresh(vcpu_t *vcpu);
void riscv_vcpu_timer_refresh_current(void);
