
/* Anonymous union includes both 32- and 64-bit names (e.g., r0/x0). */
#pragma once

#ifndef __ASSEMBLY__
#include "htypes.h"

#define __DECL_REG(n64, n32) union {            \
    uint64_t n64;                               \
    uint32_t n32;                               \
}

/* On stack VCPU state */
struct cpu_user_regs
{
    /*
     * The mapping AArch64 <-> AArch32 is based on D1.20.1 in ARM DDI
     * 0487A.d.
     *
     *         AArch64       AArch32
     */
    __DECL_REG(x0,           r0/*_usr*/);
    __DECL_REG(x1,           r1/*_usr*/);
    __DECL_REG(x2,           r2/*_usr*/);
    __DECL_REG(x3,           r3/*_usr*/);
    __DECL_REG(x4,           r4/*_usr*/);
    __DECL_REG(x5,           r5/*_usr*/);
    __DECL_REG(x6,           r6/*_usr*/);
    __DECL_REG(x7,           r7/*_usr*/);
    __DECL_REG(x8,           r8/*_usr*/);
    __DECL_REG(x9,           r9/*_usr*/);
    __DECL_REG(x10,          r10/*_usr*/);
    __DECL_REG(x11 ,         r11/*_usr*/);
    __DECL_REG(x12,          r12/*_usr*/);

    __DECL_REG(x13,          /* r13_usr */ sp_usr);
    __DECL_REG(x14,          /* r14_usr */ lr_usr);

    __DECL_REG(x15,          /* r13_hyp */ __unused_sp_hyp);

    __DECL_REG(x16,          /* r14_irq */ lr_irq);
    __DECL_REG(x17,          /* r13_irq */ sp_irq);

    __DECL_REG(x18,          /* r14_svc */ lr_svc);
    __DECL_REG(x19,          /* r13_svc */ sp_svc);

    __DECL_REG(x20,          /* r14_abt */ lr_abt);
    __DECL_REG(x21,          /* r13_abt */ sp_abt);

    __DECL_REG(x22,          /* r14_und */ lr_und);
    __DECL_REG(x23,          /* r13_und */ sp_und);

    __DECL_REG(x24,          r8_fiq);
    __DECL_REG(x25,          r9_fiq);
    __DECL_REG(x26,          r10_fiq);
    __DECL_REG(x27,          r11_fiq);
    __DECL_REG(x28,          r12_fiq);
    __DECL_REG(/* x29 */ fp, /* r13_fiq */ sp_fiq);

    __DECL_REG(/* x30 */ lr, /* r14_fiq */ lr_fiq);

    uint64_t sp; /* Valid for hypervisor frames */

    /* Return address and mode */
    __DECL_REG(pc,           pc32);             /* ELR_EL2 */
    uint64_t cpsr;                              /* SPSR_EL2 */
    uint64_t esr;                               /* ESR_EL2 */

    /* The kernel frame should be 16-byte aligned. */
    uint64_t pad0;

    /* Outer guest frame only from here on... */

    union {
        uint64_t spsr_el1;       /* AArch64 */
        uint32_t spsr_svc;       /* AArch32 */
    };

    /* AArch32 guests only */
    uint32_t spsr_fiq, spsr_irq, spsr_und, spsr_abt;

    /* AArch64 guests only */
    uint64_t sp_el0;
    uint64_t sp_el1, elr_el1;

};

#define UREGS_LR offsetof(struct cpu_user_regs, lr)
#define UREGS_SPSR_el1 offsetof(struct cpu_user_regs, spsr_el1)
#define UREGS_PC offsetof(struct cpu_user_regs, pc)
#define UREGS_CPSR offsetof(struct cpu_user_regs, cpsr)
#define UREGS_kernel_sizeof offsetof(struct cpu_user_regs, spsr_el1)

#undef __DECL_REG

#else /* for head.S */

#define UREGS_LR (8 * 30)
#define UREGS_PC (8 * 32)
#define UREGS_CPSR (8 * 33)
#define UREGS_SPSR_el1 (8 * 36)

#define UREGS_SP_el0 (8 * 39)
#define UREGS_SP_el1 (8 * 40)
#define UREGS_kernel_sizeof UREGS_SPSR_el1

#endif /* __ASSEMBLY__ */

#define PSR_THUMB       (1U <<5)      /* Thumb Mode enable */
#define PSR_FIQ_MASK    (1U <<6)      /* Fast Interrupt mask */
#define PSR_IRQ_MASK    (1U <<7)      /* Interrupt mask */
#define PSR_ABT_MASK    (1U <<8)      /* Asynchronous Abort mask */
#define PSR_BIG_ENDIAN  (1U << 9)     /* arm32: Big Endian Mode */
#define PSR_DBG_MASK    (1U << 9)     /* arm64: Debug Exception mask */
#define PSR_IT_MASK     (0x0600fc00U) /* Thumb If-Then Mask */
#define PSR_JAZELLE     (1U << 24)    /* Jazelle Mode */
#define PSR_Z           (1U << 30)    /* Zero condition flag */

#include "cpu_aarch64.h"


/* TCR_EL2 */
#define TCR_INITVAL					0x80800000
#define TCR_TBI_MASK					0x00100000
#define TCR_TBI_SHIFT					20
#define TCR_PS_MASK					0x00070000
#define TCR_PS_SHIFT					16
#define TCR_TG0_MASK					0x0000c000
#define TCR_TG0_SHIFT					14
#define TCR_SH0_MASK					0x00003000
#define TCR_SH0_SHIFT					12
#define TCR_ORGN0_MASK					0x00000C00
#define TCR_ORGN0_SHIFT					10
#define TCR_IRGN0_MASK					0x00000300
#define TCR_IRGN0_SHIFT					8
#define TCR_T0SZ_MASK					0x0000003f
#define TCR_T0SZ_SHIFT					0

#define TCR_PS_32BITS					(0 << TCR_PS_SHIFT)
#define TCR_PS_36BITS					(1 << TCR_PS_SHIFT)
#define TCR_PS_40BITS					(2 << TCR_PS_SHIFT)
#define TCR_PS_42BITS					(3 << TCR_PS_SHIFT)
#define TCR_PS_44BITS					(4 << TCR_PS_SHIFT)
#define TCR_PS_48BITS					(5 << TCR_PS_SHIFT)
#define TCR_T0SZ_VAL(in_bits)				((64 - (in_bits)) & TCR_T0SZ_MASK)
#if 0
#endif