/*
 * Test stub for atomic operations — non-SMP host, no real atomics needed.
 */

#pragma once

#include "htypes.h"

static inline long atomic_read(atomic_t *v)
{
	return v->counter;
}

static inline void atomic_set(atomic_t *v, long i)
{
	v->counter = i;
}

static inline void atomic_add(long i, atomic_t *v)
{
	v->counter += i;
}

static inline void atomic_sub(long i, atomic_t *v)
{
	v->counter -= i;
}

static inline void atomic_inc(atomic_t *v)
{
	atomic_add(1, v);
}

static inline void atomic_dec(atomic_t *v)
{
	atomic_sub(1, v);
}

static inline long atomic_add_return(long i, atomic_t *v)
{
	v->counter += i;
	return v->counter;
}

static inline long atomic_sub_return(long i, atomic_t *v)
{
	v->counter -= i;
	return v->counter;
}

static inline long atomic_inc_return(atomic_t *v)
{
	return atomic_add_return(1, v);
}

static inline long atomic_dec_return(atomic_t *v)
{
	return atomic_sub_return(1, v);
}

static inline long atomic_xchg(atomic_t *v, long n)
{
	long old = v->counter;
	v->counter = n;
	return old;
}

static inline long atomic_cmpxchg(atomic_t *v, long old, long n)
{
	long cur = v->counter;
	if (cur == old)
		v->counter = n;
	return cur;
}

static inline void atomic_set_bit(int nr, volatile unsigned long *addr)
{
	*addr |= (1UL << nr);
}

static inline void atomic_clear_bit(int nr, volatile unsigned long *addr)
{
	*addr &= ~(1UL << nr);
}

static inline int atomic_test_and_set_bit(int nr, volatile unsigned long *addr)
{
	int oldbit = (*addr >> nr) & 1;
	*addr |= (1UL << nr);
	return oldbit;
}
