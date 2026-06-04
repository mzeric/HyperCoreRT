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
#include "guest_dtb.h"
#include "riscv_features.h"
#include "plic.h"
#include "riscv_timer_manager.h"
#include "riscv_virt_irq_manager.h"

/* Per-CPU current task and global running-task snapshot (required by sched.h). */
static hyper_task_t *g_current_task[CONFIG_SMP_CPU_NUM];
static volatile int g_boot_vcpu_started;
hyper_task_t *g_running[CONFIG_SMP_CPU_NUM];

hyper_task_t *current_task(void) {
    int cpu = cpu_id();
    if (cpu < 0 || cpu >= CONFIG_SMP_CPU_NUM)
        cpu = 0;
    return g_current_task[cpu];
}

void set_current(void *c) {
    int cpu = cpu_id();
    if (cpu < 0 || cpu >= CONFIG_SMP_CPU_NUM)
        cpu = 0;
    g_current_task[cpu] = (hyper_task_t *)c;
    g_running[cpu] = (hyper_task_t *)c;
}

void riscv_mark_boot_vcpu_started(void) {
    g_boot_vcpu_started = 1;
}

int riscv_boot_vcpu_started(void) {
    return g_boot_vcpu_started;
}

/* Dummy empty regs for first switch */
static struct cpu_user_regs g_empty_regs;
static int g_task_id = 1;

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
    if (riscv_has_fpu())
        task->vcpu->regs.sstatus |= SSTATUS_FS_INITIAL;
    if (riscv_has_vector())
        task->vcpu->regs.sstatus |= SSTATUS_VS_INITIAL;
    task->vcpu->regs.sp = stack;
}

int riscv_create_guest_vcpu(u64 hartid, u64 entry, u64 a1, int priority) {
    hyper_task_t *task = (hyper_task_t *)kmalloc(sizeof(hyper_task_t));
    if (!task)
        return -1;

    memset(task, 0, sizeof(hyper_task_t));
    INIT_LIST_HEAD(&task->list);
    task->virq_lock = (spinlock_t){.lock = SPIN_UNLOCKED};

    task->priority = priority;
    task->id = g_task_id++;
    task->state = TASK_READY;
    int online_cpus = smp_cpu_count();
    if (online_cpus <= 0)
        online_cpus = 1;
    task->pcpu_affinity = (int)(hartid % (u64)online_cpus);
    task->mpidr = hartid;

    task->vcpu = create_vcpu((int)hartid, priority);
    if (!task->vcpu) {
        hyper_err("create vcpu failed");
        kfree(task);
        return -1;
    }

    uintptr_t stack_ptr = (uintptr_t)kmalloc(4096) + 4096;
    arch_vcpu_init(task->vcpu, entry, stack_ptr);
    init_task_regs(task, (void *)entry, stack_ptr);
    task->vcpu->regs.a0 = hartid;
    task->vcpu->regs.a1 = a1;

    memcpy(task->name, "guest", 5);

    hyper_info("riscv: vcpu%lu pinned to pcpu%d\n", hartid, task->pcpu_affinity);
    simple_scheduler_sched(task);
    return 0;
}

int create_task(const char *name, void *entry, int priority) {
    (void)name;
    return riscv_create_guest_vcpu(0, (u64)entry, riscv_guest_dtb_addr(), priority);
}

hyper_task_t *riscv_find_guest_vcpu(u64 hartid) {
    for (int i = 0; i < CONFIG_SMP_CPU_NUM; i++) {
        if (g_running[i] && g_running[i]->mpidr == hartid)
            return g_running[i];
    }

    arch_spin_lock(&g_sched_lock);
    hyper_task_t *task = NULL;
    list_for_each_entry(task, &g_ready_list, list) {
        if (task->mpidr == hartid) {
            arch_spin_unlock(&g_sched_lock);
            return task;
        }
    }
    arch_spin_unlock(&g_sched_lock);
    return NULL;
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
    riscv_vcpu_timer_refresh(next->vcpu);
    riscv_vplic_refresh();
    riscv_virt_irq_materialize(next->vcpu);

    /* Rearm timer for next preemption tick */
    hyp_timer_rearm();
}

void sched_yield(struct cpu_user_regs *irq_reg) {
    if (arch_get_pc(&g_empty_regs) == 0)
        g_empty_regs = *irq_reg;

    if (cpu_id() != 0 && !riscv_boot_vcpu_started())
        return;

    hyper_task_t *current = current_task();
    hyper_task_t *task = simple_scheduler_next_no_publish();

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
