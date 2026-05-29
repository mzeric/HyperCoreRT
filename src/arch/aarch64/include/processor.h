
/* Anonymous union includes both 32- and 64-bit names (e.g., r0/x0). */
#pragma once

#ifndef __ASSEMBLY__
#include "htypes.h"
#include "list.h"
#include "arch_regs.h"


#define UREGS_LR            offsetof(struct cpu_user_regs, lr)
#define UREGS_SP_L2         offsetof(struct cpu_user_regs, sp)
#define UREGS_SPSR_el1      offsetof(struct cpu_user_regs, spsr_el1)
#define UREGS_PC            offsetof(struct cpu_user_regs, pc)
#define UREGS_CPSR          offsetof(struct cpu_user_regs, cpsr)
#define UREGS_kernel_sizeof offsetof(struct cpu_user_regs, elr_el1)

#undef __DECL_REG

#else /* for head.S */

#define UREGS_LR       (8 * 30)
#define UREGS_SP_EL2   (8 * 31)
#define UREGS_PC       (8 * 32)
#define UREGS_CPSR     (8 * 33)
#define UREGS_SPSR_el1 (8 * 36)

#define UREGS_SP_el0        (8 * 39)
#define UREGS_SP_el1        (8 * 40)
#define UREGS_kernel_sizeof (8 * 42)

#endif /* __ASSEMBLY__ */

/* Field offsets for struct arch_regs */
#define ARM_ARCH_REGS_GPR0				0x0
#define ARM_ARCH_REGS_GPR1				0x8
#define ARM_ARCH_REGS_GPR2				0x10
#define ARM_ARCH_REGS_GPR3				0x18
#define ARM_ARCH_REGS_GPR4				0x20
#define ARM_ARCH_REGS_GPR5				0x28
#define ARM_ARCH_REGS_GPR6				0x30
#define ARM_ARCH_REGS_GPR7				0x38
#define ARM_ARCH_REGS_GPR8				0x40
#define ARM_ARCH_REGS_GPR9				0x48
#define ARM_ARCH_REGS_GPR10				0x50
#define ARM_ARCH_REGS_GPR11				0x58
#define ARM_ARCH_REGS_GPR12				0x60
#define ARM_ARCH_REGS_GPR13				0x68
#define ARM_ARCH_REGS_GPR14				0x70
#define ARM_ARCH_REGS_GPR15				0x78
#define ARM_ARCH_REGS_GPR16				0x80
#define ARM_ARCH_REGS_GPR17				0x88
#define ARM_ARCH_REGS_GPR18				0x90
#define ARM_ARCH_REGS_GPR19				0x98
#define ARM_ARCH_REGS_GPR20				0xa0
#define ARM_ARCH_REGS_GPR21				0xa8
#define ARM_ARCH_REGS_GPR22				0xb0
#define ARM_ARCH_REGS_GPR23				0xb8
#define ARM_ARCH_REGS_GPR24				0xc0
#define ARM_ARCH_REGS_GPR25				0xc8
#define ARM_ARCH_REGS_GPR26				0xd0
#define ARM_ARCH_REGS_GPR27				0xd8
#define ARM_ARCH_REGS_GPR28				0xe0
#define ARM_ARCH_REGS_GPR29				0xe8
#define ARM_ARCH_REGS_GPR30				0xf0
// #define ARM_ARCH_REGS_GPR31				0xf8
#define ARM_ARCH_REGS_LR				0xf0
#define ARM_ARCH_REGS_SP				0xf8
#define ARM_ARCH_REGS_PC				0x100
#define ARM_ARCH_REGS_PSTATE			0x108
#define ARM_ARCH_REGS_SIZE				0x110


#define PSR_THUMB       (1U <<5)      /* Thumb Mode enable */
#define PSR_FIQ_MASK    (1U <<6)      /* Fast Interrupt mask */
#define PSR_IRQ_MASK    (1U <<7)      /* Interrupt mask */
#define PSR_ABT_MASK    (1U <<8)      /* Asynchronous Abort mask */
#define PSR_BIG_ENDIAN  (1U << 9)     /* arm32: Big Endian Mode */
#define PSR_DBG_MASK    (1U << 9)     /* arm64: Debug Exception mask */
#define PSR_IT_MASK     (0x0600fc00U) /* Thumb If-Then Mask */
#define PSR_JAZELLE     (1U << 24)    /* Jazelle Mode */
#define PSR_Z           (1U << 30)    /* Zero condition flag */

#include "aarch64_hcr.h"

#define TCR_PS_32BITS					(0 << TCR_PS_SHIFT)
#define TCR_PS_36BITS					(1 << TCR_PS_SHIFT)
#define TCR_PS_40BITS					(2 << TCR_PS_SHIFT)
#define TCR_PS_42BITS					(3 << TCR_PS_SHIFT)
#define TCR_PS_44BITS					(4 << TCR_PS_SHIFT)
#define TCR_PS_48BITS					(5 << TCR_PS_SHIFT)
#define TCR_T0SZ_VAL(in_bits)				((64 - (in_bits)) & TCR_T0SZ_MASK)
