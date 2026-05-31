/*
 * RISC-V hypervisor scheduler — Phase 1 single-hart minimal implementation.
 *
 * Reuses sched_simple for the ready-queue, but provides RISC-V-specific
 * context switch (__vcpu_switch) that saves/restores VS-mode CSRs.
 */

#include "vmio.h"
#include "htypes.h"
#include "arch_regs.h"
#include "sched.h"
#include "inline_asm.h"
#include "mm.h"
#include "list.h"
#include <string.h>
#include "src/core/sched_simple.h"
#include "vcpu.h"
#include "timer.h"
#include "kmalloc.h"
#include "smp.h"

/* Per-CPU current task — single hart for Phase 1 */
static hyper_task_t *g_current_task;

/* Global running-task snapshot (required by sched.h) */
hyper_task_t *g_running[CONFIG_SMP_CPU_NUM];

hyper_task_t *current_task(void) { return g_current_task; }

void set_current(void *c) {
    g_current_task = (hyper_task_t *)c;
    g_running[cpu_id()] = (hyper_task_t *)c;
}

/* Dummy empty regs for first switch */
static struct cpu_user_regs g_empty_regs;

void sink_task(void) {
    hyper_task_t *task = current_task();
    hyper_info("task(%p)-%d sink", task, task->id);
    task->state = TASK_EXIT;
    kfree(task);
    set_current(NULL);
    while (1)
        ;
}

int init_sched() {
    simple_scheduler_init();
    arch_set_pc(&g_empty_regs, 0);
    return 0;
}

static void init_task_regs(hyper_task_t *task, void *entry, uintptr_t stack) {
    arch_set_return_addr(&task->vcpu->regs, (uintptr_t)sink_task);
    arch_set_pc(&task->vcpu->regs, (uintptr_t)entry);
    arch_set_task_status(&task->vcpu->regs, 0);
    task->vcpu->regs.sp = stack;
}

int create_task(const char *name, void *entry, int priority) {
    hyper_task_t *task = (hyper_task_t *)kmalloc(sizeof(hyper_task_t));
    if (!task)
        return -1;

    memset(task, 0, sizeof(hyper_task_t));
    INIT_LIST_HEAD(&task->list);
    task->virq_lock = (spinlock_t){.lock = SPIN_UNLOCKED};

    task->priority = priority;
    task->id = 1;
    task->state = TASK_READY;
    task->pcpu_affinity = 0;
    task->mpidr = 0;

    task->vcpu = create_vcpu(1, 0);
    if (!task->vcpu) {
        hyper_err("create vcpu failed");
        kfree(task);
        return -1;
    }

    uintptr_t stack_ptr = (uintptr_t)kmalloc(4096) + 4096;
    arch_vcpu_init(task->vcpu, (uintptr_t)entry, stack_ptr);
    init_task_regs(task, entry, stack_ptr);

    if (name)
        memcpy(task->name, name,
               sizeof(task->name) < strlen(name) ? sizeof(task->name) : strlen(name));

    simple_scheduler_sched(task);
    return 0;
}

/* Stubs for create_task2/3 — not needed on RISC-V Phase 1 */
int create_task2(const char *, void *, int) { return -1; }
int create_task3(const char *, void *, int) { return -1; }

static void arch_regs_restore(struct cpu_user_regs *irq_regs, vcpu_t *vcpu) {
    *irq_regs = vcpu->regs;
}

/*
 * RISC-V vCPU context switch:
 * 1. Save outgoing vCPU's VS-mode CSRs
 * 2. Restore incoming vCPU's VS-mode CSRs
 * 3. Copy vCPU GPR frame into the interrupt frame
 *
 * On return from the trap handler, head.S will:
 * - Restore GPRs from the interrupt frame
 * - Execute `sret`, which enters VS-mode (because HSTATUS.SPV=1)
 */
static void __vcpu_switch(hyper_task_t *cur, hyper_task_t *next,
                          struct cpu_user_regs *irq_regs) {
    next->switch_count++;

    if (cur) {
        /* Save outgoing vCPU state */
        cur->vcpu->regs = *irq_regs;
        vcpu_context_save(cur->vcpu);
    }

    /* Restore incoming vCPU state */
    vcpu_context_restore(next->vcpu);

    /* Copy vCPU register frame into interrupt frame for kernel_exit */
    arch_regs_restore(irq_regs, next->vcpu);

    /* Update current task tracking */
    set_current(next);

    /* Rearm timer for next preemption tick */
    hyp_timer_rearm();
}

void sched_yield(struct cpu_user_regs *irq_reg) {
    if (arch_get_pc(&g_empty_regs) == 0)
        g_empty_regs = *irq_reg;

    hyper_task_t *current = current_task();
    hyper_task_t *task = simple_scheduler_next();

    if (!task)
        return;

    if (current && current->state != TASK_EXIT)
        simple_scheduler_sched(current);

    __vcpu_switch(current, task, irq_reg);
}

/* Unused on RISC-V but required by sched.h */
void sched_yield2(struct cpu_user_regs *irq_reg) {
    sched_yield(irq_reg);
}
