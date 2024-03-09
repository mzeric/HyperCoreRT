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

#if 0
asm(
"	.irp	num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30\n"
"	.equ	.L__reg_num_x\\num, \\num\n"
"	.endr\n"
"	.equ	.L__reg_num_xzr, 31\n"
"\n"
"	.macro	mrs_s, rt, sreg\n"
"	.inst	0xd5200000|(\\sreg)|(.L__reg_num_\\rt)\n"
"	.endm\n"
"\n"
"	.macro	msr_s, sreg, rt\n"
"	.inst	0xd5000000|(\\sreg)|(.L__reg_num_\\rt)\n"
"	.endm\n"
);
#endif

/*
 * ARMv8 ARM reserves the following encoding for system registers:
 * (Ref: ARMv8 ARM, Section: "System instruction class encoding overview",
 *  C5.2, version:ARM DDI 0487A.f)
 *	[20-19] : Op0
 *	[18-16] : Op1
 *	[15-12] : CRn
 *	[11-8]  : CRm
 *	[7-5]   : Op2
 */
#define Op0_shift	19
#define Op0_mask	0x3
#define Op1_shift	16
#define Op1_mask	0x7
#define CRn_shift	12
#define CRn_mask	0xf
#define CRm_shift	8
#define CRm_mask	0xf
#define Op2_shift	5
#define Op2_mask	0x7

#define sys_reg(op0, op1, crn, crm, op2) \
	(((op0) << Op0_shift) | ((op1) << Op1_shift) | \
	 ((crn) << CRn_shift) | ((crm) << CRm_shift) | \
	 ((op2) << Op2_shift))

#define sys_reg_Op0(id)	(((id) >> Op0_shift) & Op0_mask)
#define sys_reg_Op1(id)	(((id) >> Op1_shift) & Op1_mask)
#define sys_reg_CRn(id)	(((id) >> CRn_shift) & CRn_mask)
#define sys_reg_CRm(id)	(((id) >> CRm_shift) & CRm_mask)
#define sys_reg_Op2(id)	(((id) >> Op2_shift) & Op2_mask)

#define BIT_32(nr)			(U(1) << (nr))
#define BIT_64(nr)			(ULL(1) << (nr))

#if defined(__ASSEMBLY__)
# define   U(_x)	(_x)
# define  UL(_x)	(_x)
# define ULL(_x)	(_x)
# define   L(_x)	(_x)
# define  LL(_x)	(_x)
#else
# define  U_(_x)	(_x##U)
# define   U(_x)	U_(_x)
# define  UL(_x)	(_x##UL)
# define ULL(_x)	(_x##ULL)
# define   L(_x)	(_x##L)
# define  LL(_x)	(_x##LL)

#endif