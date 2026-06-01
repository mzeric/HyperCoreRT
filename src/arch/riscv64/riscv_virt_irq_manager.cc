#include "riscv_virt_irq_manager.h"

#include "inline_asm.h"
#include "sched.h"

u64 RiscvVirtIrqManager::IrqBit(u32 irq) {
    if (irq >= 64)
        return 0;
    return 1UL << irq;
}

u64 RiscvVirtIrqManager::PendingImage(const vcpu_t *vcpu) {
    if (!vcpu)
        return 0;
    return vcpu->carch.hvip | vcpu->carch.virt_irq_pending;
}

void RiscvVirtIrqManager::Assert(vcpu_t *vcpu, u32 irq) {
    u64 bit = IrqBit(irq);
    if (!vcpu || !bit)
        return;

    vcpu->carch.virt_irq_pending |= bit;
    vcpu->carch.hvip |= bit;

    hyper_task_t *task = current_task();
    if (task && task->vcpu == vcpu)
        csrs(CSR_HVIP, bit);
}

void RiscvVirtIrqManager::Clear(vcpu_t *vcpu, u32 irq) {
    u64 bit = IrqBit(irq);
    if (!vcpu || !bit)
        return;

    vcpu->carch.virt_irq_pending &= ~bit;
    vcpu->carch.hvip &= ~bit;

    hyper_task_t *task = current_task();
    if (task && task->vcpu == vcpu)
        csrc(CSR_HVIP, bit);
}

void RiscvVirtIrqManager::Materialize(vcpu_t *vcpu) {
    if (!vcpu)
        return;

    u64 image = PendingImage(vcpu);
    vcpu->carch.hvip = image;

    hyper_task_t *task = current_task();
    if (task && task->vcpu == vcpu)
        csrw(CSR_HVIP, image);
}

void riscv_virt_irq_assert(vcpu_t *vcpu, u32 irq) {
    RiscvVirtIrqManager::Assert(vcpu, irq);
}

void riscv_virt_irq_clear(vcpu_t *vcpu, u32 irq) {
    RiscvVirtIrqManager::Clear(vcpu, irq);
}

void riscv_virt_irq_materialize(vcpu_t *vcpu) {
    RiscvVirtIrqManager::Materialize(vcpu);
}

void riscv_virt_irq_materialize_current(void) {
    hyper_task_t *task = current_task();
    if (task)
        RiscvVirtIrqManager::Materialize(task->vcpu);
}
