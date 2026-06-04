#pragma once

#include "vcpu.h"

void riscv_vcpu_context_save_manager(vcpu_t *vcpu);
void riscv_vcpu_context_restore_manager(vcpu_t *vcpu);
