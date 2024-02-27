#include "vmmio.h"
#include "htypes.h"
#include "processor.h"
#include "sched.h"
#include "cpu_inline_asm.h"
#include "mm.h"
#include "system.h"
#include "list.h"
#include <string.h>

struct list_head     g_task_list;
static hyper_task_t *g_current_task = NULL;
static size_t        g_task_id = 1;
static spinlock_t    g_task_lock;

#define INIT_PRIORITY (100u)


#define SPIN_UNLOCKED 0xffffffffUL

void enable_local_irq() { asm volatile("msr daifset, #2"); }

void disable_local_irq() { asm volatile("msr daifclr, #2"); }

void sched_preempt_enable() {}

void sched_preempt_disable() {}

int save_irq_flags() {
    int v;
    asm volatile("mrs %0, daif":"=r"(v));
    return v;
}

void restore_irq_flags(int flags) {
    asm volatile("msr daif, %0"::"r"(flags));
}

void __lock arch_spin_lock(spinlock_t *lock) {
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

void __lock arch_spin_unlock(spinlock_t *lock) {
    __asm__ __volatile__("	stlr	%w1, %0\n"
                         : "=Q"(lock->lock)
                         : "r"(SPIN_UNLOCKED)
                         : "memory");
}

#define arch_spin_lock_irqsave(lock, flags)                                                        \
    do {                                                                                           \
                                                                                                   \
        flags = save_irq_flags();                                                                   \
        disable_local_irq();                                                                       \
        sched_preempt_disable();                                                                   \
        arch_spin_lock(lock);                                                                      \
    } while (0)

#define arch_spin_unlock_irqrestore(lock, flags)                                                   \
    do {                                                                                           \
        arch_spin_unlock(lock);                                                                    \
        sched_preempt_enable();                                                                    \
        restore_irq_flags(flags);                                                                  \
    } while (0)

#define INIT_SPIN_LOCK(x) (x).lock = SPIN_UNLOCKED

hyper_task_t *current_task() { return g_current_task; }

void sink_task(void) {
    vmm_info("task-%d sink\n", current_task());
    while (1)
        ;
}

int init_sched() {
    INIT_LIST_HEAD(&g_task_list);
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

    if (name)
        memcpy(task->name, name, min(sizeof(task->name), strlen(name)));

    list_add_tail(&(task->list), &g_task_list);

    return 0;
}

void insert_task_sorted(struct list_head *head, hyper_task_t *new_task) {
    hyper_task_t     *pos;
    struct list_head *p = head;

    // 遍历链表，找到合适的插入位置
    list_for_each_entry(pos, head, list) {
        if (new_task->priority < pos->priority) {
            // 找到了插入位置
            break;
        }
        p = &pos->list;
    }

    // 插入到找到的位置之前（如果找到了比新任务priority大的任务），
    // 或者插入到具有相同priority的最后一个任务之后
    list_add_tail(&(new_task->list), p);
}

hyper_task_t *first_ready_task() {
    return list_first_entry_or_null(&g_task_list, hyper_task_t, list);
}


hyper_task_t *pick_ready_task() {
    hyper_task_t *task = first_ready_task();
    if (!task) {
        vmm_info("no task available\n");
        return NULL;
    }

    list_del(&task->list);
    task->list.next = NULL;
    task->list.prev = NULL;
    g_current_task = task;

    return task;
}

void __dump_task(struct list_head *list) {
    hyper_task_t *task = NULL;
    list_for_each_entry(task, &g_task_list, list) {
        vmm_info("probe task:%p -> %p\n", task, task->list.next);
        vmm_info("task:%s/%d p:%d\n", task->name, task->id, task->priority);
    }
}

void dump_task() { __dump_task(&g_task_list); }

void sched_insert_task(hyper_task_t *task) {
    if (!task)
        return;
    arch_spin_lock(&g_task_lock);
    insert_task_sorted(&g_task_list, task);
    arch_spin_unlock(&g_task_lock);
}

void __switch_to(hyper_task_t *cur, hyper_task_t *next, struct cpu_user_regs *irq_regs) {

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
    arch_spin_lock_irqsave(&g_task_lock, flags);

    hyper_task_t *current = current_task();
    hyper_task_t *task = pick_ready_task();
    arch_spin_unlock(&g_task_lock);

    if (!task) {
        vmm_warn("task not found\n");
        arch_spin_unlock_irqrestore(&g_task_lock, flags);
        return;
    }

    sched_insert_task(current);
    arch_spin_unlock_irqrestore(&g_task_lock, flags);

    // vmm_info("switch to task:%d\n", task->id);
    __switch_to(current, task, irq_reg);

}
