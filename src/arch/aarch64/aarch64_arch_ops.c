#include "arch_ops.h"
#include "arch_regs.h"
#include "emulate.h"
#include "hyper_config.h"
#include "kmalloc.h"
#include "vcpu.h"

static int aarch64_vcpu_create(vcpu_t *vcpu) {
    return arch_vcpu_reset(vcpu);
}

static void aarch64_vcpu_destroy(vcpu_t *vcpu) {
    if (vcpu->arch.saved_context.sp)
        kfree((void *)vcpu->arch.saved_context.sp);
}

static int aarch64_vcpu_task_init(hyper_task_t *task,
                                  const struct vm_vcpu_task_desc *desc,
                                  uintptr_t stack) {
    if (desc->guest_cpu_id != (uint64_t)-1)
        task->vcpu->arch.vmpidr = 0x80000000ULL | desc->guest_cpu_id;

#ifdef EL2_TASK
    task->vcpu->regs.sp = stack;
#else
    task->vcpu->arch.stack = (void *)stack;
#endif

    arch_set_return_addr(&task->vcpu->regs, (uintptr_t)sink_task);
    arch_set_pc(&task->vcpu->regs, desc->entry);

    if (desc->flags & VM_VCPU_TASK_F_ARG0_VALID)
        vcpu_reg_write(&task->vcpu->regs, 0, 0, desc->arg0);

    arch_set_task_status(&task->vcpu->regs, 0x3c5);
    return 0;
}

const struct arch_ops g_arch_ops = {
    .vcpu_create = aarch64_vcpu_create,
    .vcpu_destroy = aarch64_vcpu_destroy,
    .vcpu_init = arch_vcpu_init,
    .vcpu_task_init = aarch64_vcpu_task_init,
};
