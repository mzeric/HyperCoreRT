#include "include/version.h"
#define MAX_HEADER_SPACE 32

.global _reset
.global _start
.global _stack_top_
.section .text.vector

_start:
	b _head
	.asciz "HCoreRT"
	.asciz GIT_COMMIT_HASH

.if . - _start > MAX_HEADER_SPACE
    .error "space of header is too large"
.else
    .space MAX_HEADER_SPACE - (. - _start)
.endif

	.align 2

_head:
	ldr x0, =_stack_top_
	mov sp, x0

	bl _reset
	b .
