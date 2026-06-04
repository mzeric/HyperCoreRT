#include "arch_ops.h"
#include "arch_regs.h"
#include "kmalloc.h"
#include "riscv_features.h"
#include "vcpu.h"

static int riscv_vcpu_create(vcpu_t *vcpu) {
    INIT_SPIN_LOCK(vcpu->carch.virt_irq_lock);
    return 0;
}

static void riscv_vcpu_destroy(vcpu_t *vcpu) {
    if (vcpu->carch.vregs)
        kfree(vcpu->carch.vregs);
}

static int riscv_vcpu_task_init(hyper_task_t *task,
                                const struct vm_vcpu_task_desc *desc,
                                uintptr_t stack) {
    arch_set_return_addr(&task->vcpu->regs, (uintptr_t)sink_task);
    arch_set_pc(&task->vcpu->regs, desc->entry);
    arch_set_task_status(&task->vcpu->regs, 0);

    if (riscv_has_fpu())
        task->vcpu->regs.sstatus |= SSTATUS_FS_INITIAL;
    if (riscv_has_vector())
        task->vcpu->regs.sstatus |= SSTATUS_VS_INITIAL;

    task->vcpu->regs.sp = stack;
    task->vcpu->regs.a0 = desc->arg0;
    task->vcpu->regs.a1 = desc->arg1;
    return 0;
}

const struct arch_ops g_arch_ops = {
    .vcpu_create = riscv_vcpu_create,
    .vcpu_destroy = riscv_vcpu_destroy,
    .vcpu_init = arch_vcpu_init,
    .vcpu_task_init = riscv_vcpu_task_init,
};
