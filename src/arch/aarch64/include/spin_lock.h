#pragma once
#include "htypes.h"

typedef struct {
	volatile size_t lock;
} spinlock_t;

void arch_spin_lock(spinlock_t *lock);
void arch_spin_unlock(spinlock_t *lock);

void enable_local_irq(void);
void disable_local_irq(void);
int  save_irq_flags(void);
void restore_irq_flags(int flags);
void sched_preempt_enable(void);
void sched_preempt_disable(void);


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
