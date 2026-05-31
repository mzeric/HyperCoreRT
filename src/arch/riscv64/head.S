#include "regs_offset.h"
#include "macros.h"
#include "riscv64_system.h"


.global _start
.global _reset

.global _stack_top_
.global _trap_stack_top_
.global __riscv_vector
.global do_exception

.section .text.vector
_start:

    /* OpenSBI (bios default) enters here in S/HS-mode at 0x80000000 */

    /* init stack */
    la t0, _stack_top_

	// setup sscratch
	la	a5, _trap_stack_top_
	li	a3, RISCV_SCRATCH_SIZE
	sub a5, a5, a3
	csrw CSR_SSCRATCH, a5
	csrw CSR_VSSCRATCH, a5

	// setup trap stack
	csrr	tp, CSR_SSCRATCH
	sd tp, RISCV_SCRATCH_EXCE_STACK_OFFSET(tp)

    la sp, _stack_top_
    j _reset

    /* should never be here */
    j .


.align 4
__riscv_vector:

	/* Swap TP and SSCRATCH */
	csrrw	tp, CSR_SSCRATCH, tp

	sd sp, RISCV_SCRATCH_TMP0_OFFSET(tp)
	ld sp, RISCV_SCRATCH_EXCE_STACK_OFFSET(tp)

	kernel_entry

	// save real sp
	ld s0, RISCV_SCRATCH_TMP0_OFFSET(tp)
	sd s0, PT_SP(sp)

	csrrw tp, CSR_SSCRATCH, tp
	sd tp, PT_TP(sp)

	la ra, ret_from_exception

	mv a0, sp /* cpu_user_regs */
	mv a1, s4

	tail do_exception
ret_from_exception:

restore_all:

	kernel_exit
	sret
