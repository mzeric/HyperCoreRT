#pragma once

#include "htypes.h"
#include "vcpu.h"

void riscv_timer_arm(vcpu_t *vcpu, u64 deadline_cycles);
void riscv_timer_clear(vcpu_t *vcpu);
void riscv_timer_refresh(vcpu_t *vcpu);
void riscv_vcpu_timer_arm_current(u64 deadline_cycles);
void riscv_vcpu_timer_clear_current(void);
void riscv_vcpu_timer_refresh(vcpu_t *vcpu);
void riscv_vcpu_timer_refresh_current(void);
