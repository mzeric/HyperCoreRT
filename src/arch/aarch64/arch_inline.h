#pragma once

#include "htypes.h"
#include "system.h"

#define SPINLOCK_UNLOCK (0)

static inline void arch_spin_lock(spinlock_t *lock)
{
	u32 cpu = smp_id();
	unsigned long tmp;

	__asm__ __volatile__(
"	sevl\n"
"1:	wfe\n"
"2:	ldaxr	%w0, %1\n"
"	cmp	%w0, %w3\n"
"	b.ne	1b\n"
"	stxr	%w0, %w2, %1\n"
"	cbnz	%w0, 2b\n"
	: "=&r" (tmp), "+Q" (lock->lock)
	: "r" (cpu), "r" (SPINLOCK_UNLOCK)
	: "cc", "memory");
}

static inline void arch_spin_unlock(spinlock_t *lock)
{
	__asm__ __volatile__(
"	stlr	%w1, %0\n"
	: "=Q" (lock->lock) : "r" (SPINLOCK_UNLOCK)
	: "memory");
}
