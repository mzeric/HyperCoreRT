#pragma once

#include "sched.h"

struct arch_ops {
    int  (*vcpu_create)(vcpu_t *vcpu);
    void (*vcpu_destroy)(vcpu_t *vcpu);
    int  (*vcpu_init)(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack);
    int  (*vcpu_task_init)(hyper_task_t *task,
                           const struct vm_vcpu_task_desc *desc,
                           uintptr_t stack);
};

extern const struct arch_ops g_arch_ops;
