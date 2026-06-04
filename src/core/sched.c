#include "vmio.h"
#include "htypes.h"
#include "arch_regs.h"
#include "sched.h"
#include "inline_asm.h"
#include "mm.h"
#include "list.h"
#include <string.h>
#include "stdbool.h"
#include "sched_simple.h"
#include "vcpu.h"
#include <ioremap.h>
#include "emul_gic.h"
#include "emulate.h"
#include "timer.h"
#include "src/drivers/gic/gicv3.h"
#include "hyper_config.h"
#include "percpu.h"

DEFINE_PER_CPU(hyper_task_t *, current_task);

/* Global running-task snapshot, updated by set_current(). */
hyper_task_t *g_running[CONFIG_SMP_CPU_NUM];

// static spinlock_t           g_task_lock;
static struct cpu_user_regs g_empty_regs;

#define INIT_PRIORITY (100u)


hyper_task_t *current_task(void) {
    return *(hyper_task_t **)this_cpu(current_task);
}

void set_current(void *c) {
    *(hyper_task_t **)this_cpu(current_task) = (hyper_task_t *)c;
    g_running[cpu_id()] = (hyper_task_t *)c;
}

void sink_task(void) {
    hyper_task_t *task = current_task();
    hyper_info("task(%p)-%d sink", task, task->id);
    task->state = TASK_EXIT;
    kfree(task);
    set_current(NULL);

    hyper_info("sink done");
    // wfi();
    while (1)
        ;
}

int init_sched() {
    simple_scheduler_init();
    // INIT_SPIN_LOCK(g_task_lock);
    // g_empty_regs.pc = 0;
    arch_set_pc(&g_empty_regs, 0);

    return 0;
}

int create_task(const char *name, void *entry, int priority) {
    u64 boot_mpidr = hyper_config()->guest.vcpu_mpidr[0] & 0xff00ffffffULL;
    struct vm_vcpu_task_desc desc = {
        .name = name,
        .vcpu_id = 1,
        .priority = priority,
        .vcpu_priority = 0,
        .pcpu_affinity = 0,
        .entry = (uintptr_t)entry,
        .arg0 = hyper_config()->guest.dtb_addr,
        .guest_cpu_id = boot_mpidr,
        .flags = VM_VCPU_TASK_F_ARG0_VALID,
    };

    return vm_create_vcpu_task(&desc);
}

//For smp
int create_task2(const char *name, void *entry, int priority) {
    /* mpidr is patched in by psci_vcpu_on right after create_task2 returns */
    struct vm_vcpu_task_desc desc = {
        .name = name,
        .vcpu_id = 1,
        .priority = priority,
        .vcpu_priority = 0,
        .pcpu_affinity = 0,
        .entry = (uintptr_t)entry,
        .guest_cpu_id = (uint64_t)-1,
    };

    return vm_create_vcpu_task(&desc);
}

static void mTaskEntry(void)
{
    unsigned long off = 0x12341234;

    asm volatile("msr tpidr_el1, %0"
			:: "r" (off) : "memory");

    unsigned long output = 0;

    asm volatile("mrs %0, tpidr_el1" : "=r" (output) ::);
    while(1);
}

int create_task3(const char *name, void *__entry, int priority) {
    void *entry = (void *)&mTaskEntry;
    struct vm_vcpu_task_desc desc = {
        .name = name,
        .vcpu_id = 1,
        .priority = priority,
        .vcpu_priority = 0,
        .pcpu_affinity = 0,
        .entry = (uintptr_t)entry,
        .guest_cpu_id = (uint64_t)-1,
    };

    (void)__entry;
    return vm_create_vcpu_task(&desc);
}

void __dump_task(struct list_head *list) {
    hyper_task_t *task = NULL;
    list_for_each_entry(task, &g_ready_list, list) {
        hyper_info("task:%s/%d p:%d", task->name, task->id, task->priority);
    }
}

void dump_task() { __dump_task(&g_ready_list); }

void arch_regs_restore(struct cpu_user_regs *irq_regs, vcpu_t *vcpu) {

    /* dont touch EL2 regs */
    vcpu->regs.sp = irq_regs->sp;

    *irq_regs = vcpu->regs;

}

void __el2_switch_to(hyper_task_t *cur, hyper_task_t *next, struct cpu_user_regs *irq_regs) {

    next->switch_count++;
    if (next->switch_count <= 2)
        hyper_info("switch on pCPU%d: task%d '%s' -> task%d '%s' (pc=0x%lx)",
                   cpu_id(),
                   cur ? cur->id : 0, cur ? cur->name : "(none)",
                   next->id, next->name,
                   next->vcpu->regs.pc);

    if (cur) {
        /* save cur frame to task info
            in the first switch cur is NULL
        */

        // cur->regs = *irq_regs;

        cur->vcpu->regs = *irq_regs;
        gic_vcpu_save(cur->vcpu);
        vcpu_context_save(cur->vcpu);
    }
    /* restore next to irq_regs */
    // *irq_regs = next->regs;

    vcpu_context_restore(next->vcpu);
    gic_vcpu_restore(next->vcpu);

    /* Guest GIC init can disable the EL2 physical timer PPI. */
    if (cur)
        gicv3_reenable_hyp_timer_ppi();

    /* TLB flush skipped — all vCPUs share the same stage-1 (kernel) page
       tables and the same stage-2 page tables, so stale entries from the
       previous vCPU are still valid. */

    arch_regs_restore(irq_regs, next->vcpu);

    gicv3_reenable_hyp_timer_ppi();
    hyp_timer_rearm();

    /* push pending vIRQs into ICH_LR before we ERET back to EL1 */
    gic_vcpu_flush_lr(next);

}

void sched_yield(struct cpu_user_regs *irq_reg) {

    if (arch_get_pc(&g_empty_regs) == 0)
        g_empty_regs = *irq_reg;
    hyper_task_t *current = current_task();

    hyper_task_t *task = simple_scheduler_next();

    if (!task) {
        /* No other task to run — flush pending virtual interrupts for the
         * current vCPU so that cross-pCPU SGIs are delivered without
         * requiring a full context switch. */
        if (current && current->pending_virq_count > 0)
            gic_vcpu_flush_lr(current);
        return;
    }

    if (current && current->state != TASK_EXIT)
        simple_scheduler_sched(current);
    // arch_spin_unlock_irqrestore(&g_task_lock, flags);
    __el2_switch_to(current, task, irq_reg);
}

void sched_yield2(struct cpu_user_regs *irq_reg) {

    if (arch_get_pc(&g_empty_regs) == 0)
        g_empty_regs = *irq_reg;
    hyper_task_t *current = current_task();
    // arch_spin_lock_irqsave(&g_task_lock, flags);

    hyper_task_t *task = simple_scheduler_next();

    if (!task) {
        // hyper_warn("task not found, current:%p", current);
        // arch_spin_unlock_irqrestore(&g_task_lock, flags);
        // if (current)  __el2_switch_to(NULL, current, irq_reg);
        // *irq_reg = current->vcpu->regs;
        return;
    }

    // if (current && current->state != TASK_EXIT)
    //     simple_scheduler_sched(current);
    // arch_spin_unlock_irqrestore(&g_task_lock, flags);
    __el2_switch_to(current, current, irq_reg);

}
