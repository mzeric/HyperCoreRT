#pragma once

#include "htypes.h"
#include "vcpu.h"

void riscv_virt_irq_assert(vcpu_t *vcpu, u32 irq);
void riscv_virt_irq_clear(vcpu_t *vcpu, u32 irq);
void riscv_virt_irq_materialize(vcpu_t *vcpu);
void riscv_virt_irq_materialize_current(void);
u64 riscv_virt_irq_pending_image(vcpu_t *vcpu);
