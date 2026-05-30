/*
 * RISC-V memory barrier primitives.
 *
 * Thin wrappers over RISC-V FENCE / FENCE.I instructions used by
 * the rest of HyperCoreRT.
 */

#pragma once

/*
 * Raw fence instructions.
 *
 * FENCE predecessor, successor — orders memory accesses before the
 * fence with those after the fence.
 * FENCE.I — instruction-fetch fence (syncs i-cache with d-cache).
 */
#define fence()    __asm__ __volatile__ ("fence"     : : : "memory")
#define fence_i()  __asm__ __volatile__ ("fence.i"   : : : "memory")

/*
 * Full barriers.
 *
 * FENCE rw, rw — full memory barrier (all loads and stores).
 * FENCE r, r   — load-load / load-acquire barrier.
 * FENCE w, w   — store-store barrier.
 */
#define arch_mb()      __asm__ __volatile__ ("fence rw, rw" : : : "memory")
#define arch_rmb()     __asm__ __volatile__ ("fence r, r"   : : : "memory")
#define arch_wmb()     __asm__ __volatile__ ("fence w, w"   : : : "memory")

/*
 * SMP barriers — on RISC-V these are the same as the full barriers
 * because FENCE already has the desired scope.
 */
#define arch_smp_mb()  arch_mb()
#define arch_smp_rmb() arch_rmb()
#define arch_smp_wmb() arch_wmb()

/* Hint for spin loops; yield the current hart. */
#define arch_cpu_relax() __asm__ __volatile__ ("" : : : "memory")
