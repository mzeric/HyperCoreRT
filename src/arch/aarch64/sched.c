#include "vmmio.h"
#include "htypes.h"
#include "processor.h"
#include "sched.h"
#include "cpu_inline_asm.h"
#include "mm.h"
#include "system.h"
#include "list.h"
#include <string.h>
#include "vmmlib.h"
#include "sched_simple.h"


hyper_task_t     *g_current_task = NULL;
static size_t     g_task_id = 1;
static spinlock_t g_task_lock;

#define INIT_PRIORITY (100u)

void enable_local_irq() { asm volatile("msr daifset, #2"); }

void disable_local_irq() { asm volatile("msr daifclr, #2"); }

void sched_preempt_enable() {}

void sched_preempt_disable() {}

int save_irq_flags() {
    int v;
    asm volatile("mrs %0, daif" : "=r"(v));
    return v;
}

void restore_irq_flags(int flags) { asm volatile("msr daif, %0" ::"r"(flags)); }

void arch_spin_lock(spinlock_t *lock) {
    u32           cpu = smp_id();
    unsigned long tmp;

    __asm__ __volatile__("	sevl\n"
                         "1:	wfe\n"
                         "2:	ldaxr	%w0, %1\n"
                         "	cmp	%w0, %w3\n"
                         "	b.ne	1b\n"
                         "	stxr	%w0, %w2, %1\n"
                         "	cbnz	%w0, 2b\n"
                         : "=&r"(tmp), "+Q"(lock->lock)
                         : "r"(cpu), "r"(SPIN_UNLOCKED)
                         : "cc", "memory");
}

void arch_spin_unlock(spinlock_t *lock) {
    __asm__ __volatile__("	stlr	%w1, %0\n"
                         : "=Q"(lock->lock)
                         : "r"(SPIN_UNLOCKED)
                         : "memory");
}

#define INIT_SPIN_LOCK(x) (x).lock = SPIN_UNLOCKED

hyper_task_t *current_task() { return g_current_task; }

void sink_task(void) {
    hyper_task_t *task = current_task();
    vmm_info("task(%p)-%d sink\n", task, task->id);
    task->state = TASK_EXIT;
    kfree(task);
    g_current_task = NULL;

    vmm_info("sink done\n");
    while(1){
        wfi();
    }
}

int init_sched() {
    simple_scheduler_init();
    g_current_task = NULL;
    INIT_SPIN_LOCK(g_task_lock);

    return 0;
}

int init_task_regs(hyper_task_t *task, void *entry) {
    task->regs.sp = (uintptr_t)kmalloc(4096) + 4096;
    task->regs.pc = (uintptr_t)entry;
    task->regs.lr = (uintptr_t)sink_task;
    task->regs.cpsr = PSR_MODE64_EL2h;

    return 0;
}

int create_task(const char *name, void *entry, int priority) {
    hyper_task_t *task;

    task = (hyper_task_t *)kmalloc(sizeof(hyper_task_t));

    memset(task, 0, sizeof(hyper_task_t));
    INIT_LIST_HEAD(&task->list);

    init_task_regs(task, entry);

    task->priority = priority;
    task->id = g_task_id++;
    task->state = TASK_READY;

    if (name)
        memcpy(task->name, name, min(sizeof(task->name), strlen(name)));

    simple_scheduler_sched(task);

    return 0;
}

void __dump_task(struct list_head *list) {
    hyper_task_t *task = NULL;
    list_for_each_entry(task, &g_ready_list, list) {
        vmm_info("task:%s/%d p:%d\n", task->name, task->id, task->priority);
    }
}

void dump_task() { __dump_task(&g_ready_list); }

void __el2_switch_to(hyper_task_t *cur, hyper_task_t *next, struct cpu_user_regs *irq_regs) {

    vmm_info("switch %p -> %p\n", cur, next);
    if (cur) {
        /* save cur frame to task info
            in the first switch cur is NULL
        */
        cur->regs = *irq_regs;
    }
    /* restore next to irq_regs */
    *irq_regs = next->regs;
    // memcpy(irq_regs, &next->regs, sizeof(struct cpu_user_regs));
}

void sched_yield(struct cpu_user_regs *irq_reg) {

    int flags;

    dump_task();
    hyper_task_t *current = current_task();

    arch_spin_lock_irqsave(&g_task_lock, flags);
    hyper_task_t *task = simple_scheduler_next();

    if (!task) {
        // vmm_warn("task not found\n");
        arch_spin_unlock_irqrestore(&g_task_lock, flags);
        return;
    }

    if (current && current->state != TASK_EXIT)
        simple_scheduler_sched(current);
    arch_spin_unlock_irqrestore(&g_task_lock, flags);

    // vmm_info("switch to task:%d\n", task->id);
    __el2_switch_to(current, task, irq_reg);
}
