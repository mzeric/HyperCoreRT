#pragma once
#include "htypes.h"

#define SBI_CALL(which, arg0, arg1, arg2) ({			\
	register u64 a0 asm ("a0") = (u64)(arg0);	\
	register u64 a1 asm ("a1") = (u64)(arg1);	\
	register u64 a2 asm ("a2") = (u64)(arg2);	\
	register u64 a7 asm ("a7") = (u64)(which);	\
	asm volatile ("ecall"					\
		      : "+r" (a0)				\
		      : "r" (a1), "r" (a2), "r" (a7)		\
		      : "memory");				\
	a0;							\
})

/*
 * 陷入到M模式，调用M模式提供的服务。
 * SBI运行到M模式下
 */
#define SBI_CALL_0(which) SBI_CALL(which, 0, 0, 0)
#define SBI_CALL_1(which, arg0) SBI_CALL(which, arg0, 0, 0)
#define SBI_CALL_2(which, arg0, arg1) SBI_CALL(which, arg0, arg1, 0)

#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 0x1
#define SBI_CONSOLE_GETCHAR 0x2