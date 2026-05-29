#include "spin_lock.h"
#include "aarch64_system.h"

void enable_local_irq() { asm volatile("msr daifclr, #2"); }

void disable_local_irq() { asm volatile("msr daifset, #2"); }

void sched_preempt_enable() {}

void sched_preempt_disable() {}


int save_irq_flags() {
    int v;
    asm volatile("mrs %0, daif" : "=r"(v));
    return v;
}

void restore_irq_flags(int flags) {
    asm volatile("msr daif, %0" ::"r"(flags));
}

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