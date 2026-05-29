/*
 * AArch64 atomic operations.
 *
 * Uses LL/SC (ldxr/stxr) loops — compatible with v8.0 (cortex-a57)
 * without LSE atomic instructions.
 *
 * Memory ordering: all mutating operations use stlxr (store-release
 * exclusive) + dmb(ish) to provide full acquire/release semantics
 * between CPUs.
 */

#pragma once

#include "htypes.h"
#include "arch_barrier.h"

/*
 * Basic read / write.
 * volatile access is sufficient — no special barrier needed
 * for a single-word aligned read on AArch64.
 */
static inline long atomic_read(atomic_t *v)
{
	return *(volatile long *)&v->counter;
}

static inline void atomic_set(atomic_t *v, long i)
{
	*(volatile long *)&v->counter = i;
}

/* ---- add / sub (no return) ---- */

static inline void atomic_add(long i, atomic_t *v)
{
	unsigned long tmp;

	__asm__ __volatile__(
	"1:	ldxr	%0, %1\n"
	"	add	%0, %0, %2\n"
	"	stlxr	%w0, %0, %1\n"
	"	cbnz	%w0, 1b\n"
	"	dmb	ish\n"
	: "=&r" (tmp), "+Q" (v->counter)
	: "r" (i)
	: "memory");
}

static inline void atomic_sub(long i, atomic_t *v)
{
	unsigned long tmp;

	__asm__ __volatile__(
	"1:	ldxr	%0, %1\n"
	"	sub	%0, %0, %2\n"
	"	stlxr	%w0, %0, %1\n"
	"	cbnz	%w0, 1b\n"
	"	dmb	ish\n"
	: "=&r" (tmp), "+Q" (v->counter)
	: "r" (i)
	: "memory");
}

static inline void atomic_inc(atomic_t *v)
{
	atomic_add(1, v);
}

static inline void atomic_dec(atomic_t *v)
{
	atomic_sub(1, v);
}

/* ---- add / sub with return ---- */

static inline long atomic_add_return(long i, atomic_t *v)
{
	unsigned long tmp, result;

	__asm__ __volatile__(
	"1:	ldxr	%0, %2\n"
	"	add	%0, %0, %3\n"
	"	stlxr	%w1, %0, %2\n"
	"	cbnz	%w1, 1b\n"
	"	dmb	ish\n"
	: "=&r" (result), "=&r" (tmp), "+Q" (v->counter)
	: "r" (i)
	: "memory");

	return (long)result;
}

static inline long atomic_sub_return(long i, atomic_t *v)
{
	unsigned long tmp, result;

	__asm__ __volatile__(
	"1:	ldxr	%0, %2\n"
	"	sub	%0, %0, %3\n"
	"	stlxr	%w1, %0, %2\n"
	"	cbnz	%w1, 1b\n"
	"	dmb	ish\n"
	: "=&r" (result), "=&r" (tmp), "+Q" (v->counter)
	: "r" (i)
	: "memory");

	return (long)result;
}

static inline long atomic_inc_return(atomic_t *v)
{
	return atomic_add_return(1, v);
}

static inline long atomic_dec_return(atomic_t *v)
{
	return atomic_sub_return(1, v);
}

/* ---- exchange / compare-exchange ---- */

static inline long atomic_xchg(atomic_t *v, long n)
{
	unsigned long tmp, result;

	__asm__ __volatile__(
	"1:	ldxr	%0, %2\n"
	"	stlxr	%w1, %3, %2\n"
	"	cbnz	%w1, 1b\n"
	"	dmb	ish\n"
	: "=&r" (result), "=&r" (tmp), "+Q" (v->counter)
	: "r" (n)
	: "memory");

	return (long)result;
}

static inline long atomic_cmpxchg(atomic_t *v, long old, long n)
{
	unsigned long tmp, result;

	__asm__ __volatile__(
	"1:	ldxr	%0, %2\n"
	"	mov	%w1, #0\n"
	"	cmp	%0, %3\n"
	"	b.ne	2f\n"
	"	stlxr	%w1, %4, %2\n"
	"	cbnz	%w1, 1b\n"
	"2:	dmb	ish\n"
	: "=&r" (result), "=&r" (tmp), "+Q" (v->counter)
	: "r" (old), "r" (n)
	: "cc", "memory");

	return (long)result;
}

/* ---- bit operations ---- */

static inline void atomic_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long tmp, val;
	unsigned long mask = 1UL << nr;

	__asm__ __volatile__(
	"1:	ldxr	%0, %2\n"
	"	orr	%0, %0, %3\n"
	"	stlxr	%w1, %0, %2\n"
	"	cbnz	%w1, 1b\n"
	"	dmb	ish\n"
	: "=&r" (val), "=&r" (tmp), "+Q" (*addr)
	: "r" (mask)
	: "memory");
}

static inline void atomic_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long tmp, val;
	unsigned long mask = 1UL << nr;

	__asm__ __volatile__(
	"1:	ldxr	%0, %2\n"
	"	bic	%0, %0, %3\n"
	"	stlxr	%w1, %0, %2\n"
	"	cbnz	%w1, 1b\n"
	"	dmb	ish\n"
	: "=&r" (val), "=&r" (tmp), "+Q" (*addr)
	: "r" (mask)
	: "memory");
}

static inline int atomic_test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long tmp, val;
	unsigned long mask = 1UL << nr;
	int oldbit;

	__asm__ __volatile__(
	"1:	ldxr	%0, %2\n"
	"	lsr	%1, %0, %4\n"
	"	and	%w1, %w1, #1\n"
	"	orr	%0, %0, %3\n"
	"	stlxr	%w5, %0, %2\n"
	"	cbnz	%w5, 1b\n"
	"	dmb	ish\n"
	: "=&r" (val), "=&r" (oldbit), "+Q" (*addr)
	: "r" (mask), "r" ((unsigned long)nr)
	: "memory");

	return oldbit;
}
