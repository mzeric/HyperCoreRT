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

hyper_task_t               *g_current_task = NULL;
static size_t               g_task_id = 1;
// static spinlock_t           g_task_lock;
static struct cpu_user_regs g_empty_regs;

#define INIT_PRIORITY (100u)


hyper_task_t *current_task() { return g_current_task; }

void set_current(void *c) { g_current_task = c; }

void sink_task(void) {
    hyper_task_t *task = current_task();
    hyper_info("task(%p)-%d sink\n", task, task->id);
    task->state = TASK_EXIT;
    kfree(task);
    g_current_task = NULL;

    hyper_info("sink done\n");
    // wfi();
    while (1)
        ;
}

int init_sched() {
    simple_scheduler_init();
    g_current_task = NULL;
    // INIT_SPIN_LOCK(g_task_lock);
    // g_empty_regs.pc = 0;
    arch_set_pc(&g_empty_regs, 0);

    return 0;
}

int init_task_regs(hyper_task_t *task, void *entry, uintptr_t stack) {
#if 0
    task->regs.sp = (uintptr_t)kmalloc(4096) + 4096;
    task->regs.pc = (uintptr_t)entry;
    task->regs.lr = (uintptr_t)sink_task;
    task->regs.cpsr = PSR_MODE64_EL2h;
#else

#ifdef EL2_TASK
    task->vcpu->regs.sp = stack;// sp_el2
#else
    task->vcpu->arch.stack = (void *)stack; // sp_el1;
#endif

#if 0
    task->vcpu->regs.pc = (uintptr_t)entry;
    task->vcpu->regs.lr = (uintptr_t)sink_task;
    task->vcpu->regs.cpsr = 0x3c5;
#else
    arch_set_return_addr(&task->vcpu->regs, (uintptr_t)sink_task);
    arch_set_pc(&task->vcpu->regs, (uintptr_t)entry);

    vcpu_reg_write(&task->vcpu->regs, 0, 0, hyper_config()->guest.dtb_addr);

    arch_set_task_status(&task->vcpu->regs, 0x3c5);

#endif

#endif


    return 0;
}
    extern void *_guest_stack_end;

int create_task(const char *name, void *entry, int priority) {
    hyper_task_t *task;

    task = (hyper_task_t *)kmalloc(sizeof(hyper_task_t));

    memset(task, 0, sizeof(hyper_task_t));
    INIT_LIST_HEAD(&task->list);

    hyper_debug("create vcpu:%p\n", task->vcpu);

    task->priority = priority;
    task->id = g_task_id++;
    task->state = TASK_READY;
    u64 boot_mpidr = hyper_config()->guest.vcpu_mpidr[0] & 0xff00ffffffULL;
    task->mpidr = boot_mpidr;
    task->vcpu = create_vcpu(1, 0);
    if (task->vcpu)
        task->vcpu->arch.vmpidr = 0x80000000ULL | boot_mpidr;
    if (task->vcpu == NULL) {
        hyper_err("create vcpu failed\n");
        return -1;
    }
    uintptr_t stack_ptr = (uintptr_t)kmalloc(4096) + 4096;
    hyper_info("init task stack:%p\n", (void *)stack_ptr);
    arch_vcpu_init(task->vcpu, (uintptr_t)entry, stack_ptr);
    init_task_regs(task, entry, stack_ptr);
    if (name)
        memcpy(task->name, name, min(sizeof(task->name), strlen(name)));

    hyper_info("init done\n");
    simple_scheduler_sched(task);

    return 0;
}

//For smp
int init_task_regs2(hyper_task_t *task, void *entry, uintptr_t stack) {
#if 0
    task->regs.sp = (uintptr_t)kmalloc(4096) + 4096;
    task->regs.pc = (uintptr_t)entry;
    task->regs.lr = (uintptr_t)sink_task;
    task->regs.cpsr = PSR_MODE64_EL2h;
#else

#ifdef EL2_TASK
    task->vcpu->regs.sp = stack;// sp_el2
#else
    task->vcpu->arch.stack = (void *)stack; // sp_el1;
#endif

#if 0
    task->vcpu->regs.pc = (uintptr_t)entry;
    task->vcpu->regs.lr = (uintptr_t)sink_task;
    task->vcpu->regs.cpsr = 0x3c5;
#else
    arch_set_return_addr(&task->vcpu->regs, (uintptr_t)sink_task);
    arch_set_pc(&task->vcpu->regs, (uintptr_t)entry);

    arch_set_task_status(&task->vcpu->regs, 0x3c5);

#endif

#endif


    return 0;
}

int create_task2(const char *name, void *entry, int priority) {
    hyper_task_t *task;

    task = (hyper_task_t *)kmalloc(sizeof(hyper_task_t));

    memset(task, 0, sizeof(hyper_task_t));
    INIT_LIST_HEAD(&task->list);

    hyper_debug("create vcpu:%p\n", task->vcpu);

    task->priority = priority;
    task->id = g_task_id++;
    task->state = TASK_READY;
    /* mpidr is patched in by psci_vcpu_on right after create_task2 returns */
    task->mpidr = (uint64_t)-1;
    task->vcpu = create_vcpu(1, 0);
    if (task->vcpu == NULL) {
        hyper_err("create vcpu failed\n");
        return -1;
    }
    uintptr_t stack_ptr = (uintptr_t)kmalloc(4096) + 4096;
    hyper_info("init task stack:%p\n", (void *)stack_ptr);
    arch_vcpu_init(task->vcpu, (uintptr_t)entry, stack_ptr);
    init_task_regs2(task, entry, stack_ptr);
    if (name)
        memcpy(task->name, name, min(sizeof(task->name), strlen(name)));

    hyper_info("init done\n");
    simple_scheduler_sched(task);

    return 0;
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
    hyper_task_t *task;

    void *entry = &mTaskEntry;

    task = (hyper_task_t *)kmalloc(sizeof(hyper_task_t));

    memset(task, 0, sizeof(hyper_task_t));
    INIT_LIST_HEAD(&task->list);

    hyper_debug("create vcpu:%p\n", task->vcpu);

    task->priority = priority;
    task->id = g_task_id++;
    task->state = TASK_READY;
    task->vcpu = create_vcpu(1, 0);
    if (task->vcpu == NULL) {
        hyper_err("create vcpu failed\n");
        return -1;
    }
    uintptr_t stack_ptr = (uintptr_t)kmalloc(4096) + 4096;
    hyper_info("init task stack:%p\n", (void *)stack_ptr);
    arch_vcpu_init(task->vcpu, (uintptr_t)entry, stack_ptr);
    init_task_regs2(task, entry, stack_ptr);
    if (name)
        memcpy(task->name, name, min(sizeof(task->name), strlen(name)));

    hyper_info("init done\n");
    simple_scheduler_sched(task);

    return 0;
}

void __dump_task(struct list_head *list) {
    hyper_task_t *task = NULL;
    list_for_each_entry(task, &g_ready_list, list) {
        hyper_info("task:%s/%d p:%d\n", task->name, task->id, task->priority);
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
    // arch_spin_lock_irqsave(&g_task_lock, flags);

    hyper_task_t *task = simple_scheduler_next();

    if (!task) {
        // hyper_warn("task not found, current:%p\n", current);
        // arch_spin_unlock_irqrestore(&g_task_lock, flags);
        // if (current)  __el2_switch_to(NULL, current, irq_reg);
        // *irq_reg = current->vcpu->regs;
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
        // hyper_warn("task not found, current:%p\n", current);
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
