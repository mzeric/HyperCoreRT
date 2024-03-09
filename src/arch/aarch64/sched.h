#pragma once
#include "list.h"
#include "processor.h"
#include "vcpu.h"


enum task_state {
    TASK_RUNNING = 1,
    TASK_PAUSE,
    TASK_READY,
    TASK_EXIT,
};
typedef struct hyper_task {
    struct cpu_user_regs regs;
    int                  id;
    int                  priority;
    char                 name[8];
    struct list_head     list;
    enum task_state      state;
    vcpu_t              *vcpu;
} hyper_task_t;

typedef struct {
	volatile size_t lock;
} spinlock_t;

int init_sched();
int create_task(const char *name, void *entry, int priority);

void sched_yield(struct cpu_user_regs *irq_reg);
hyper_task_t *current_task();

void arch_spin_lock(spinlock_t *lock);
void arch_spin_unlock(spinlock_t *lock);

int  save_irq_flags();
void restore_irq_flags(int flags);
void sched_preempt_enable();
void sched_preempt_disable();

// #define SPIN_UNLOCKED 0xBEAF

#define SPIN_UNLOCKED 0u

#define arch_spin_lock_irqsave(lock, flags)                                                        \
    do {                                                                                           \
                                                                                                   \
        flags = save_irq_flags();                                                                  \
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
