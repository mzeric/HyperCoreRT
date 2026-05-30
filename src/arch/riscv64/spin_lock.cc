#include "spin_lock.h"
#include "inline_asm.h"

void enable_local_irq(void) {
    csrs(sstatus, 0x2u); /* SSTATUS_SIE */
}

void disable_local_irq(void) {
    csrc(sstatus, 0x2u);
}

void sched_preempt_enable(void) {}
void sched_preempt_disable(void) {}

int save_irq_flags(void) {
    return (int)csrr(sstatus);
}

void restore_irq_flags(int flags) {
    csrw(sstatus, (unsigned long)flags);
}

void arch_spin_lock(spinlock_t *lock) {
    unsigned long tmp;
    __asm__ __volatile__(
        "1: lr.w %0, %1        \n"
        "   bnez %0, 1b        \n"
        "   sc.w %0, %2, %1    \n"
        "   bnez %0, 1b        \n"
        : "=&r"(tmp), "+A"(lock->lock)
        : "r"(1u)
        : "memory");
}

void arch_spin_unlock(spinlock_t *lock) {
    __asm__ __volatile__(
        "sw zero, %0           \n"
        : "=A"(lock->lock)
        :
        : "memory");
}
