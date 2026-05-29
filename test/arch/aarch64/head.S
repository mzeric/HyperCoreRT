#include "processor.h"
#include "aarch64_system.h"
#include "asm_macros.h"

#define MAX_HEADER_SPACE 32

#define IFLAGS_D_BIT    8
#define IFLAGS_A_BIT    4
#define IFLAGS_I_BIT    2
#define IFLAGS_F_BIT    1
#define MAX_HEADER_SPACE 32
/*
 * Short-hands to define the interrupts (D, A, I, F)
 *
 * _ means the interrupt state will not change
 * X means the state of interrupt X will change
 *
 * To be used with msr daif{set, clr} only.
 *
 */
#define IFLAGS__AI_     IFLAGS_A_BIT | IFLAGS_I_BIT
#define IFLAGS__A__     IFLAGS_A_BIT
#define IFLAGS___I_     IFLAGS_I_BIT

.global _reset
.global _start
.global _hang_head
.global _stack_top_
.global on_guest_excep_return

.section .text.vector

_start:
	b _head

.if . - _start > MAX_HEADER_SPACE
    .error "space of header is too large"
.else
    .space MAX_HEADER_SPACE - (. - _start)
.endif

	.align 2
_guest_head:
        b .

_head:
	ldr x0, =_hvc_stack_end
	mov sp, x0

	bl _reset
	b .

        .macro  push, xreg1, xreg2
        stp     \xreg1, \xreg2, [sp, #-16]!
        .endm

        .macro  pop, xreg1, xreg2
        ldp     \xreg1, \xreg2, [sp], #16
        .endm

.macro debug_asm
        mov     X0, #0x50000000
        mov     X1, #'H'
        str      X1, [X0]
        b .
.endm
/*
 * Save/restore guest mode specific state, outer stack frame
 */
        .macro  excep_entry_guest

        add     x21, sp, #UREGS_SPSR_el1
        mrs     x23, SPSR_el1
        str     x23, [x21]

        add     x21, sp, #UREGS_SP_el0
        mrs     x22, SP_el0
        str     x22, [x21]

        add     x21, sp, #UREGS_SP_el1
        mrs     x22, SP_el1
        mrs     x23, ELR_el1
        stp     x22, x23, [x21]
       .endm

        .macro  excep_return_guest

        add     x21, sp, #UREGS_SPSR_el1
        ldr     x23, [x21]
        msr     SPSR_el1, x23

        add     x21, sp, #UREGS_SP_el0
        ldr     x22, [x21]
        msr     SP_el0, x22

        add     x21, sp, #UREGS_SP_el1
        ldp     x22, x23, [x21]
        msr     SP_el1, x22
        msr     ELR_el1, x23

        .endm
/*
 * Save state on entry to hypervisor, restore on exit
 *
 * save_x0_x1: Does the macro needs to save x0/x1? Defaults to 1
 * If 0, we rely on the on x0/x1 to have been saved at the correct
 * position on the stack before.
 */
        .macro  excep_entry, hyp=1, save_x0_x1=1

        /*
         * Ensure any PAR_EL1 reads complete, in case we were interrupted
         * between the PAR_EL1 read and the memory barrier for the erratum
         * 1508412 workaround.
         */
        dmb sy

	sub	sp, sp, #ARM_ARCH_REGS_SIZE
	stp	x28, x29, [sp, #ARM_ARCH_REGS_GPR28]	/* save GPR0-GPR29 */
	stp	x26, x27, [sp, #ARM_ARCH_REGS_GPR26]
	stp	x24, x25, [sp, #ARM_ARCH_REGS_GPR24]
	stp	x22, x23, [sp, #ARM_ARCH_REGS_GPR22]
	stp	x20, x21, [sp, #ARM_ARCH_REGS_GPR20]
	stp	x18, x19, [sp, #ARM_ARCH_REGS_GPR18]
	stp	x16, x17, [sp, #ARM_ARCH_REGS_GPR16]
	stp	x14, x15, [sp, #ARM_ARCH_REGS_GPR14]
	stp	x12, x13, [sp, #ARM_ARCH_REGS_GPR12]
	stp	x10, x11, [sp, #ARM_ARCH_REGS_GPR10]
	stp	x8, x9, [sp, #ARM_ARCH_REGS_GPR8]
	stp	x6, x7, [sp, #ARM_ARCH_REGS_GPR6]
	stp	x4, x5, [sp, #ARM_ARCH_REGS_GPR4]
	stp	x2, x3, [sp, #ARM_ARCH_REGS_GPR2]
	stp	x0, x1, [sp, #ARM_ARCH_REGS_GPR0]
	add	x29, sp, #ARM_ARCH_REGS_SIZE		/* save LR, SP */
	stp	x30, x29, [sp, #ARM_ARCH_REGS_LR]
	mrs	x22, elr_el1				/* save ELR, SPSR */
	mrs	x23, spsr_el1
	stp	x22, x23, [sp, #ARM_ARCH_REGS_PC]


        .endm
return_from_trap:

       //msr     daifset, #IFLAGS___I_ /* Mask interrupts */

	ldp	x22, x23, [sp, #ARM_ARCH_REGS_PC]	/* load ELR, SPSR */
	msr	elr_el1, x22
	msr	spsr_el1, x23
	ldp	x30, x28, [sp, #ARM_ARCH_REGS_LR]	/* load LR, SP */
	mov	x29, sp
	mov	sp, x28
	ldp	x0, x1, [x29, #ARM_ARCH_REGS_GPR0]	/* load GPR0-GPR29 */
	ldp	x2, x3, [x29, #ARM_ARCH_REGS_GPR2]
	ldp	x4, x5, [x29, #ARM_ARCH_REGS_GPR4]
	ldp	x6, x7, [x29, #ARM_ARCH_REGS_GPR6]
	ldp	x8, x9, [x29, #ARM_ARCH_REGS_GPR8]
	ldp	x10, x11, [x29, #ARM_ARCH_REGS_GPR10]
	ldp	x12, x13, [x29, #ARM_ARCH_REGS_GPR12]
	ldp	x14, x15, [x29, #ARM_ARCH_REGS_GPR14]
	ldp	x16, x17, [x29, #ARM_ARCH_REGS_GPR16]
	ldp	x18, x19, [x29, #ARM_ARCH_REGS_GPR18]
	ldp	x20, x21, [x29, #ARM_ARCH_REGS_GPR20]
	ldp	x22, x23, [x29, #ARM_ARCH_REGS_GPR22]
	ldp	x24, x25, [x29, #ARM_ARCH_REGS_GPR24]
	ldp	x26, x27, [x29, #ARM_ARCH_REGS_GPR26]
	ldp	x28, x29, [x29, #ARM_ARCH_REGS_GPR28]

        eret
        /*sb*/

.global __hyp_vectors
/*
 * Exception vectors.
 */
        .macro  ventry  label
        .align  7
        b       \label
        .endm

        .align  11
__hyp_vectors:
        ventry  hyp_sync_invalid            /* Synchronous EL2t */
        ventry  hyp_irq_invalid             /* IRQ EL2t */
        ventry  hyp_fiq_invalid             /* FIQ EL2t */
        ventry  hyp_error_invalid           /* Error EL2t */

        ventry  hyp_sync                    /* Synchronous EL2h */
        ventry  hyp_irq                     /* IRQ EL2h */
        ventry  hyp_fiq_invalid             /* FIQ EL2h */
        ventry  hyp_error                   /* Error EL2h */

        ventry  guest_sync                  /* Synchronous 64-bit EL0/EL1 */
        ventry  guest_irq                   /* IRQ 64-bit EL0/EL1 */
        ventry  guest_fiq_invalid           /* FIQ 64-bit EL0/EL1 */
        ventry  guest_error                 /* Error 64-bit EL0/EL1 */

        ventry  guest_sync_compat           /* Synchronous 32-bit EL0/EL1 */
        ventry  guest_irq_compat            /* IRQ 32-bit EL0/EL1 */
        ventry  guest_fiq_invalid_compat    /* FIQ 32-bit EL0/EL1 */
        ventry  guest_error_compat          /* Error 32-bit EL0/EL1 */

.macro  call_excep, func, is_compat=0
mov     x0, sp
mov     x1, \is_compat
bl       \func
.endm

.macro excep_return, hyp=1

.if \hyp == 0
        bl on_guest_excep_return
        excep_return_guest
.endif
b return_from_trap
.endm


hyp_sync_invalid:
hyp_irq_invalid:
hyp_fiq_invalid:
hyp_error_invalid:
		excep_entry
		call_excep do_bad_mode, #0
                excep_return

hyp_sync:


               /* Inherit interrupts */
        mrs     x0, SPSR_el1
        and     x0, x0, #(PSR_DBG_MASK | PSR_ABT_MASK | PSR_IRQ_MASK | PSR_FIQ_MASK)
        msr     daif, x0
        excep_entry
                call_excep do_hyper_sync
        excep_return
hyp_irq:
                #mov x25, #'E'
                #mov x24, #0x1c090000
                #str x25, [x24]
                #eret
                excep_entry
		call_excep do_irq_mode, #1
                excep_return
hyp_error:

                excep_entry
		call_excep do_bad_mode, #1
                excep_return

guest_sync:
                excep_entry 0
		call_excep do_guest_exception 2
                excep_return
guest_irq:
                excep_entry 0
		call_excep do_guest_irq
                excep_return
guest_fiq_invalid:
                excep_entry 0
		call_excep do_guest_exception 4
                excep_return
guest_error:
                excep_entry 0
		call_excep do_guest_exception 5
                excep_return

guest_sync_compat:
guest_irq_compat:
guest_fiq_invalid_compat:
guest_error_compat:
                excep_entry 0
		call_excep do_guest_exception, 1
                excep_return
