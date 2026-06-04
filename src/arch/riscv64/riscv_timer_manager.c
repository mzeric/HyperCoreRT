#include "riscv_timer_manager.h"

#include "exception.h"
#include "riscv_virt_irq_manager.h"
#include "sched.h"
#include "timer.h"

void riscv_timer_arm(vcpu_t *vcpu, u64 deadline_cycles) {
    if (!vcpu)
        return;

    vcpu->carch.timer.deadline_cycles = deadline_cycles;
    vcpu->carch.timer.armed = 1;
    vcpu->carch.timer.pending = 0;
    riscv_virt_irq_clear(vcpu, IRQ_VS_TIMER);
    riscv_timer_refresh(vcpu);
}

void riscv_timer_clear(vcpu_t *vcpu) {
    if (!vcpu)
        return;

    vcpu->carch.timer.armed = 0;
    vcpu->carch.timer.pending = 0;
    riscv_virt_irq_clear(vcpu, IRQ_VS_TIMER);
}

void riscv_timer_refresh(vcpu_t *vcpu) {
    if (!vcpu || !vcpu->carch.timer.armed)
        return;
    if (get_cycles() < vcpu->carch.timer.deadline_cycles)
        return;

    vcpu->carch.timer.armed = 0;
    vcpu->carch.timer.pending = 1;
    riscv_virt_irq_assert(vcpu, IRQ_VS_TIMER);
}

static vcpu_t *riscv_current_vcpu(void) {
    hyper_task_t *task = current_task();
    return task ? task->vcpu : NULL;
}

void riscv_vcpu_timer_arm_current(u64 deadline_cycles) {
    riscv_timer_arm(riscv_current_vcpu(), deadline_cycles);
}

void riscv_vcpu_timer_clear_current(void) {
    riscv_timer_clear(riscv_current_vcpu());
}

void riscv_vcpu_timer_refresh(vcpu_t *vcpu) {
    riscv_timer_refresh(vcpu);
}

void riscv_vcpu_timer_refresh_current(void) {
    riscv_timer_refresh(riscv_current_vcpu());
}
