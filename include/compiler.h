/*
 * Compiler / toolchain helpers used across HyperCoreRT.
 *
 * Everything here is a thin wrapper around a GCC builtin or attribute
 * plus a couple of arithmetic / bit-twiddling helpers that are
 * convenient enough to live alongside them.  No code is generated;
 * including this header is free at runtime.
 *
 * Sections:
 *   1. Stringification + the no-op compiler barrier
 *   2. Function attributes
 *   3. Linker section attributes
 *   4. Branch hints
 *   5. Bit / arithmetic helpers
 *   6. AArch64 system-register encoding
 *   7. Integer-literal suffix helpers
 */

#pragma once

/* -------------------------------------------------------------- *
 * 1. Stringification + compiler barrier                          *
 * -------------------------------------------------------------- */

#define tostring(_s...)   #_s
#define stringify(_s...)  tostring(_s)

/* Memory clobber forces the compiler to spill caches across this point. */
#define barrier()         __asm__ __volatile__ ("" ::: "memory")

#ifndef offsetof
#define offsetof(_t, _m)  __builtin_offsetof(_t, _m)
#endif


/* -------------------------------------------------------------- *
 * 2. Function attributes                                         *
 * -------------------------------------------------------------- */

#ifndef __noinline
#define __noinline        __attribute__((__noinline__))
#endif
#ifndef __always_inline
#define __always_inline   inline __attribute__((__always_inline__))
#endif
#define __unused          __attribute__((__unused__))
#define __maybe_unused    __attribute__((__unused__))
#define __used            __attribute__((__used__))
#ifndef __aligned
#define __aligned(_n)     __attribute__((__aligned__(_n)))
#endif
#define __noreturn        __attribute__((__noreturn__))
#define __notrace         __attribute__((__no_instrument_function__))
#define __packed          __attribute__((__packed__))
#define __weak            __attribute__((__weak__))
#define __mustcheck       __attribute__((__warn_unused_result__))
#define __printf(_f, _a)  __attribute__((__format__(printf, (_f), (_a))))
#define __naked           __attribute__((__signal__, __naked__))


/* -------------------------------------------------------------- *
 * 3. Linker section attributes                                   *
 * -------------------------------------------------------------- */

#ifndef __section
#define __section(_s)     __attribute__((__section__(#_s)))
#endif

#define __read_mostly     __section(.readmostly.data)
#define __lock            __section(.spinlock.text)
#define __modtbl          __section(.modtbl)
#define __nidtbl          __section(.nidtbl)
#define __symtbl          __section(.symtbl)
#define __percpu          __section(.percpu)

#define __init            __section(.init.text)
#define __initconst       __section(.init.rodata)
#define __initdata        __section(.init.data)
#define __exit            /* nothing (kept for symmetry with __init) */

#define __cpuinit         __section(.cpuinit.text)
#define __cpuexit         /* nothing */


/* -------------------------------------------------------------- *
 * 4. Branch hints                                                *
 * -------------------------------------------------------------- */

#define likely(_x)        __builtin_expect(!!(_x), 1)
#define unlikely(_x)      __builtin_expect(!!(_x), 0)


/* -------------------------------------------------------------- *
 * 5. Bit / arithmetic helpers                                    *
 *                                                                *
 * The __ff* / __fl* family returns a zero-based bit index of the *
 * first / last set bit in the operand.  We rebase the result of  *
 * the corresponding GCC builtin so that bit 0 maps to 0 (the     *
 * builtins themselves are 1-based, with 0 meaning "no bit set"). *
 * -------------------------------------------------------------- */

/* First set bit, counted from LSB. */
#define __ffs(_x)         (__builtin_ffs((_x))  - 1)
#define __ffsl(_x)        (__builtin_ffsl((_x)) - 1)

/* Last set bit, counted from LSB. */
#define __fls(_x)         ((sizeof(_x) * 8) - __builtin_clz((_x))  - 1)
#define __flsl(_x)        ((sizeof(_x) * 8) - __builtin_clzl((_x)) - 1)

/* Silence -Wunused for a single value. */
#define MARK_UNUSED(_x)   ((void)(_x))

/* Align _v up to the next boundary, where _m is (alignment - 1). */
#define ALIGN_MASK(_v, _m)   (((_v) + (_m)) & ~(_m))

/* Round-down / round-up to a multiple of _q, preserving the type of _v. */
#define __round_mask(_v, _q)  ((__typeof__(_v))((_q) - 1))
#define round_down(_v, _q)    ((_v) & ~__round_mask(_v, _q))
#define round_up(_v, _q)      ((((_v) - 1) | __round_mask(_v, _q)) + 1)


/* -------------------------------------------------------------- *
 * 6. AArch64 system-register encoding                            *
 *                                                                *
 * ARMv8 packs (Op0, Op1, CRn, CRm, Op2) into a 21-bit field that *
 * is OR-ed into the MRS_S/MSR_S instruction encoding so that     *
 * registers without a mnemonic (or those introduced after the    *
 * toolchain was built) can still be reached.                     *
 *   bits  [20:19]  Op0                                           *
 *   bits  [18:16]  Op1                                           *
 *   bits  [15:12]  CRn                                           *
 *   bits  [11: 8]  CRm                                           *
 *   bits  [ 7: 5]  Op2                                           *
 * (See ARM ARM, "System instruction class encoding overview".)   *
 * -------------------------------------------------------------- */

#define Op0_shift  19
#define Op0_mask   0x3
#define Op1_shift  16
#define Op1_mask   0x7
#define CRn_shift  12
#define CRn_mask   0xf
#define CRm_shift  8
#define CRm_mask   0xf
#define Op2_shift  5
#define Op2_mask   0x7

#define sys_reg(_op0, _op1, _crn, _crm, _op2)         \
        ( ((_op0) << Op0_shift)                        \
        | ((_op1) << Op1_shift)                        \
        | ((_crn) << CRn_shift)                        \
        | ((_crm) << CRm_shift)                        \
        | ((_op2) << Op2_shift) )

#define sys_reg_Op0(_id)  (((_id) >> Op0_shift) & Op0_mask)
#define sys_reg_Op1(_id)  (((_id) >> Op1_shift) & Op1_mask)
#define sys_reg_CRn(_id)  (((_id) >> CRn_shift) & CRn_mask)
#define sys_reg_CRm(_id)  (((_id) >> CRm_shift) & CRm_mask)
#define sys_reg_Op2(_id)  (((_id) >> Op2_shift) & Op2_mask)


/* -------------------------------------------------------------- *
 * 7. Integer-literal suffix helpers                              *
 *                                                                *
 * Used to tag bitfield definitions so they have the right type   *
 * in both C and inline-assembly source.  Inside .S files the     *
 * suffixes would confuse the assembler, hence the __ASSEMBLY__   *
 * fallback that strips them.                                     *
 * -------------------------------------------------------------- */

#if defined(__ASSEMBLY__)
#  define U(_x)            (_x)
#  define UL(_x)           (_x)
#  define ULL(_x)          (_x)
#  define L(_x)            (_x)
#  define LL(_x)           (_x)
#else
#  define U_(_x)           (_x ## U)
#  define U(_x)            U_(_x)
#  define UL(_x)           (_x ## UL)
#  define ULL(_x)          (_x ## ULL)
#  define L(_x)            (_x ## L)
#  define LL(_x)           (_x ## LL)
#endif

#define BIT_32(_n)         (U(1)   << (_n))
#define BIT_64(_n)         (ULL(1) << (_n))
