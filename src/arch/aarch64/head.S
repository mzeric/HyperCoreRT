#include "version.h"
#include "processor.h"
#define MAX_HEADER_SPACE 32

#define IFLAGS_D_BIT    8
#define IFLAGS_A_BIT    4
#define IFLAGS_I_BIT    2
#define IFLAGS_F_BIT    1

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
	.asciz "HCoreRT"
	.asciz GIT_COMMIT_HASH

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

	sub     sp, sp,  #(UREGS_SPSR_el1 - UREGS_LR) /* CPSR, PC, SP, LR */

        push    x28, x29
        push    x26, x27
        push    x24, x25
        push    x22, x23
        push    x20, x21
        push    x18, x19
        push    x16, x17
        push    x14, x15
        push    x12, x13
        push    x10, x11
        push    x8, x9
        push    x6, x7
        push    x4, x5
        push    x2, x3


        push    x0, x1

        .if \hyp == 1        /* Hypervisor mode */

        add     x21, sp, #UREGS_kernel_sizeof

        .else                /* Guest mode */

        excep_entry_guest
        mov     x21, ~0 /* sp only valid for hyp frame XXX */

        .endif

        stp     lr, x21, [sp, #UREGS_LR]

        mrs     x21, elr_el2
        str     x21, [sp, #UREGS_PC]

        add     x21, sp, #UREGS_CPSR
        mrs     x22, spsr_el2
        mrs     x23, esr_el2
        stp     x22, x23, [x21]

        .endm
return_from_trap:
        msr     daifset, #IFLAGS___I_ /* Mask interrupts */

        ldr     x21, [sp, #UREGS_PC]            /* load ELR */
        ldr     x22, [sp, #UREGS_CPSR]          /* load SPSR */

        pop     x0, x1
        pop     x2, x3
        pop     x4, x5
        pop     x6, x7
        pop     x8, x9

        msr     elr_el2, x21                    /* set up the return data */
        msr     spsr_el2, x22

        pop     x10, x11
        pop     x12, x13
        pop     x14, x15
        pop     x16, x17
        pop     x18, x19
        pop     x20, x21
        pop     x22, x23
        pop     x24, x25
        pop     x26, x27
        pop     x28, x29

        ldr     lr, [sp], #(UREGS_SPSR_el1 - UREGS_LR) /* CPSR, PC, SP, LR */

        /*
         * Ensure any device/NC reads complete, in case we were interrupted
         * between the memory barrier for the erratum 1508412 workaround and
         * any PAR_EL1 read.
        alternative_if ARM64_WORKAROUND_1508412
        alternative_else_nop_endif
         */
        dmb sy

        eret
        /*sb*/

.global __vmm_vectors
/*
 * Exception vectors.
 */
        .macro  ventry  label
        .align  7
        b       \label
        .endm

        .align  11
__vmm_vectors:
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
        mrs     x0, SPSR_el2
        and     x0, x0, #(PSR_DBG_MASK | PSR_ABT_MASK | PSR_IRQ_MASK | PSR_FIQ_MASK)
        msr     daif, x0

        excep_entry
                call_excep do_hyper_sync
        excep_return
hyp_irq:
                excep_entry
		call_excep do_irq_mode, #1
                excep_return
hyp_error:
                excep_entry
		call_excep do_bad_mode, #1
                excep_return

guest_sync:
guest_irq:
guest_fiq_invalid:
guest_error:
                excep_entry 0
		call_excep do_guest_exception
                excep_return

guest_sync_compat:
guest_irq_compat:
guest_fiq_invalid_compat:
guest_error_compat:
                excep_entry 0
		call_excep do_guest_exception, 1
                excep_return

