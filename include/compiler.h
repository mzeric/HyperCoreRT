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
#define __packed		__attribute__((packed))
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