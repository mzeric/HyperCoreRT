#include "arch_ops.h"
#include "kmalloc.h"
#include "sched.h"
#include "sched_simple.h"
#include "vcpu.h"
#include "vmio.h"
#include <string.h>

static int g_vcpu_task_id = 1;

static void vm_task_copy_name(hyper_task_t *task, const char *name) {
    if (!name)
        return;

    size_t len = strlen(name);
    if (len >= sizeof(task->name))
        len = sizeof(task->name) - 1;
    memcpy(task->name, name, len);
    task->name[len] = '\0';
}

int vm_create_vcpu_task(const struct vm_vcpu_task_desc *desc) {
    if (!desc || !g_arch_ops.vcpu_init || !g_arch_ops.vcpu_task_init)
        return -1;

    hyper_task_t *task = (hyper_task_t *)kmalloc(sizeof(hyper_task_t));
    if (!task)
        return -1;

    memset(task, 0, sizeof(hyper_task_t));
    INIT_LIST_HEAD(&task->list);
    task->virq_lock = (spinlock_t){.lock = SPIN_UNLOCKED};
    task->priority = desc->priority;
    task->id = g_vcpu_task_id++;
    task->state = TASK_READY;
    task->pcpu_affinity = desc->pcpu_affinity;
    task->mpidr = desc->guest_cpu_id;
    vm_task_copy_name(task, desc->name);

    task->vcpu = create_vcpu(desc->vcpu_id, desc->vcpu_priority);
    if (!task->vcpu) {
        hyper_err("create vcpu failed");
        kfree(task);
        return -1;
    }

    uintptr_t stack_base = (uintptr_t)kmalloc(VM_VCPU_TASK_STACK_SIZE);
    if (!stack_base) {
        destroy_vcpu(task->vcpu);
        kfree(task);
        return -1;
    }

    uintptr_t stack = stack_base + VM_VCPU_TASK_STACK_SIZE;
    hyper_info("init task stack:%p", (void *)stack);

    if (g_arch_ops.vcpu_init(task->vcpu, desc->entry, stack) != 0 ||
        g_arch_ops.vcpu_task_init(task, desc, stack) != 0) {
        kfree((void *)stack_base);
        destroy_vcpu(task->vcpu);
        kfree(task);
        return -1;
    }

    hyper_info("init done");
    simple_scheduler_sched(task);
    return 0;
}
