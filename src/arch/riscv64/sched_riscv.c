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

int riscv_create_guest_vcpu(u64 hartid, u64 entry, u64 a1, int priority) {
    int online_cpus = smp_cpu_count();
    if (online_cpus <= 0)
        online_cpus = 1;
    int pcpu_affinity = (int)(hartid % (u64)online_cpus);
    struct vm_vcpu_task_desc desc = {
        .name = "guest",
        .vcpu_id = (int)hartid,
        .priority = priority,
        .vcpu_priority = priority,
        .pcpu_affinity = pcpu_affinity,
        .entry = entry,
        .arg0 = hartid,
        .arg1 = a1,
        .guest_cpu_id = hartid,
    };

    int ret = vm_create_vcpu_task(&desc);
    if (ret == 0)
        hyper_info("riscv: vcpu%lu pinned to pcpu%d\n", hartid, pcpu_affinity);
    return ret;
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
