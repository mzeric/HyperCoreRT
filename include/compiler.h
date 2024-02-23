#pragma once

#define tostring(s...)		#s
#define stringify(s...)		tostring(s)
#define barrier() 		__asm__ __volatile__("": : :"memory")
#define offsetof(a,b) __builtin_offsetof(a,b)

#define __noinline		__attribute__ ((noinline))
#define __always_inline 	inline __attribute__((always_inline))
#define __unused		__attribute__((unused))
#define __maybe_unused		__attribute__((unused))
#define __used			__attribute__((used))
#define __aligned(x)		__attribute__((aligned(x)))
#define __noreturn		__attribute__((noreturn))
#define __notrace		__attribute__((no_instrument_function))
#define __packed		__attribute__((__packed__))
#define __weak			__attribute__((weak))
#define __mustcheck		__attribute__((warn_unused_result))
#define __printf(a, b)		__attribute__((format(printf, a, b)))

#define __section(S)		__attribute__((section(#S)))
#define __read_mostly		__section(".readmostly.data")
#define __lock			__section(".spinlock.text")
#define __modtbl		__section(".modtbl")
#define __nidtbl		__section(".nidtbl")
#define __symtbl		__section(".symtbl")
#define __percpu		__section(".percpu")
#define __init			__section(".init.text")
#define __initconst		__section(".init.rodata")
#define __initdata		__section(".init.data")
#define __exit

#define __cpuinit		__section(".cpuinit.text")
#define __cpuexit

#define __naked __attribute__((signal, naked))

#define likely(x)     __builtin_expect(!!(x),1)
#define unlikely(x)   __builtin_expect(!!(x),0)

/*
    some compile like aarch64-none-elf provide 'fls/ffs' function ,
     __ffs/__fls is zero-based 0-31

    Note: the index return by __ffs/__fls start from 1
*/

/*  find first (least-significant) set bit, starting at the least significant bit position,  equ __builtin_ctz */
#define __ffs(x)  (__builtin_ffs((x)) - 1)
#define __ffsl(x) (__builtin_ffsl((x)) - 1)

/* find last   (most-significant)  set bit, starting at the least significant bit position */
#define __fls(x)  ((sizeof(x) * 8) - __builtin_clz((x)) - 1)
#define __flsl(x) ((sizeof(x) * 8) - __builtin_clzl((x)) - 1)

#define MARK_UNUSED(x) (void)(x)

#define ALIGN_MASK(x, mask)           (((x) + (mask)) & ~(mask))

/*
 * (typeof x)y need to be as wide as x
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_down(x, y)   ((x) & ~__round_mask(x, y))
#define round_up(x, y)     ((((x)-1) | __round_mask(x, y)) + 1)
