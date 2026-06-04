#include "riscv_virt_irq_manager.h"

#include "inline_asm.h"
#include "sched.h"

static u64 riscv_virt_irq_bit(u32 irq) {
    if (irq >= 64)
        return 0;
    return 1UL << irq;
}

u64 riscv_virt_irq_pending_image(vcpu_t *vcpu) {
    if (!vcpu)
        return 0;

    int flags;
    arch_spin_lock_irqsave(&vcpu->carch.virt_irq_lock, flags);
    u64 image = vcpu->carch.virt_irq_pending;
    arch_spin_unlock_irqrestore(&vcpu->carch.virt_irq_lock, flags);
    return image;
}

void riscv_virt_irq_assert(vcpu_t *vcpu, u32 irq) {
    u64 bit = riscv_virt_irq_bit(irq);
    if (!vcpu || !bit)
        return;

    int flags;
    arch_spin_lock_irqsave(&vcpu->carch.virt_irq_lock, flags);
    vcpu->carch.virt_irq_pending |= bit;

    hyper_task_t *task = current_task();
    if (task && task->vcpu == vcpu)
        csrs(CSR_HVIP, bit);
    arch_spin_unlock_irqrestore(&vcpu->carch.virt_irq_lock, flags);
}

void riscv_virt_irq_clear(vcpu_t *vcpu, u32 irq) {
    u64 bit = riscv_virt_irq_bit(irq);
    if (!vcpu || !bit)
        return;

    int flags;
    arch_spin_lock_irqsave(&vcpu->carch.virt_irq_lock, flags);
    vcpu->carch.virt_irq_pending &= ~bit;

    hyper_task_t *task = current_task();
    if (task && task->vcpu == vcpu)
        csrc(CSR_HVIP, bit);
    arch_spin_unlock_irqrestore(&vcpu->carch.virt_irq_lock, flags);
}

void riscv_virt_irq_materialize(vcpu_t *vcpu) {
    if (!vcpu)
        return;

    int flags;
    arch_spin_lock_irqsave(&vcpu->carch.virt_irq_lock, flags);
    u64 image = vcpu->carch.virt_irq_pending;

    hyper_task_t *task = current_task();
    if (task && task->vcpu == vcpu)
        csrw(CSR_HVIP, image);
    arch_spin_unlock_irqrestore(&vcpu->carch.virt_irq_lock, flags);
}

void riscv_virt_irq_materialize_current(void) {
    hyper_task_t *task = current_task();
    if (task)
        riscv_virt_irq_materialize(task->vcpu);
}
