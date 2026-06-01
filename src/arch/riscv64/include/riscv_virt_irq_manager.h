#pragma once

#include "htypes.h"
#include "vcpu.h"

class RiscvVirtIrqManager final {
public:
    static void Assert(vcpu_t *vcpu, u32 irq);
    static void Clear(vcpu_t *vcpu, u32 irq);
    static void Materialize(vcpu_t *vcpu);
    static u64 PendingImage(const vcpu_t *vcpu);

private:
    RiscvVirtIrqManager() = delete;
    static u64 IrqBit(u32 irq);
};

void riscv_virt_irq_assert(vcpu_t *vcpu, u32 irq);
void riscv_virt_irq_clear(vcpu_t *vcpu, u32 irq);
void riscv_virt_irq_materialize(vcpu_t *vcpu);
void riscv_virt_irq_materialize_current(void);
