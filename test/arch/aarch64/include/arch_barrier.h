/*
 * ARM64 memory barrier primitives.
 *
 * Thin wrappers over the AArch64 ISB/DMB/DSB instructions and the
 * single-direction / SMP variants used by the rest of HyperCoreRT.
 */

#pragma once

/*
 * Raw barrier instructions.
 *
 * - isb takes no operand (its only AArch64 form is the unconditional
 *   instruction-synchronisation barrier).
 * - dmb/dsb are parameterised by the shareability/access scope token
 *   (sy, ish, ishld, ishst, ld, st, ...).
 */
#define isb()      __asm__ __volatile__ ("isb"           : : : "memory")
#define dmb(scope) __asm__ __volatile__ ("dmb " #scope   : : : "memory")
#define dsb(scope) __asm__ __volatile__ ("dsb " #scope   : : : "memory")

/*
 * Full-system barriers (dsb sy fences both loads and stores against
 * every observer in the system).
 */
#define arch_mb()      dsb(sy)
#define arch_rmb()     dsb(ld)
#define arch_wmb()     dsb(st)

/*
 * Inner-shareable variants for SMP ordering between CPUs in the
 * same coherency domain (used in lock fast paths, etc.).
 */
#define arch_smp_mb()  dmb(ish)
#define arch_smp_rmb() dmb(ishld)
#define arch_smp_wmb() dmb(ishst)

/* Hint for spin loops; here we only need to keep the optimiser honest. */
#define arch_cpu_relax() __asm__ __volatile__ ("" : : : "memory")
