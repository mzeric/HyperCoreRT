
.global _reset
.global _start
.global _stack_top_
.section .text.vector

_start:

	ldr x0, =_stack_top_
	mov sp, x0

	bl _reset
	b .
