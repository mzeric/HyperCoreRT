/*
 * AArch64 system-register bit definitions used across HyperCoreRT.
 *
 * Values are taken from the ARM Architecture Reference Manual
 * (DDI 0487).  The file groups constants by their owning register so
 * that callers can include the whole thing and pick what they need.
 *
 * Single-bit positions are written in the explicit (1UL << n) form
 * so the values are obviously hex-equivalent; multi-bit fields and
 * read masks keep their hex form.
 *
 * Sections:
 *   CTR_EL0, MIDR_EL1, MPIDR_EL1, TTBCR (legacy),
 *   SCTLR_EL{1,2}, HCR_EL2,
 *   TCR_EL1, VTCR_EL2,
 *   HCPTR / HSTR / HDCR (AArch32),
 *   HSR_EC syndrome codes,
 *   FSR / PAR, VMID, generic-timer control,
 *   exception-vector layout, PSTATE.M encodings.
 */

#pragma once

/* CTR Cache Type Register */
#define CTR_L1IP_MASK       0x3
#define CTR_L1IP_SHIFT      14
#define CTR_DMINLINE_SHIFT  16
#define CTR_IMINLINE_SHIFT  0
#define CTR_IMINLINE_MASK   0xf
#define CTR_ERG_SHIFT       20
#define CTR_CWG_SHIFT       24
#define CTR_CWG_MASK        15
#define CTR_IDC_SHIFT       28
#define CTR_DIC_SHIFT       29

#define ICACHE_POLICY_VPIPT  0
#define ICACHE_POLICY_AIVIVT 1
#define ICACHE_POLICY_VIPT   2
#define ICACHE_POLICY_PIPT   3

/* MIDR Main ID Register */
#define MIDR_REVISION_MASK      0xf
#define MIDR_RESIVION(midr)     ((midr) & MIDR_REVISION_MASK)
#define MIDR_PARTNUM_SHIFT      4
#define MIDR_PARTNUM_MASK       (0xfff << MIDR_PARTNUM_SHIFT)
#define MIDR_PARTNUM(midr) \
    (((midr) & MIDR_PARTNUM_MASK) >> MIDR_PARTNUM_SHIFT)
#define MIDR_ARCHITECTURE_SHIFT 16
#define MIDR_ARCHITECTURE_MASK  (0xf << MIDR_ARCHITECTURE_SHIFT)
#define MIDR_ARCHITECTURE(midr) \
    (((midr) & MIDR_ARCHITECTURE_MASK) >> MIDR_ARCHITECTURE_SHIFT)
#define MIDR_VARIANT_SHIFT      20
#define MIDR_VARIANT_MASK       (0xf << MIDR_VARIANT_SHIFT)
#define MIDR_VARIANT(midr) \
    (((midr) & MIDR_VARIANT_MASK) >> MIDR_VARIANT_SHIFT)
#define MIDR_IMPLEMENTOR_SHIFT  24
#define MIDR_IMPLEMENTOR_MASK   (0xffU << MIDR_IMPLEMENTOR_SHIFT)
#define MIDR_IMPLEMENTOR(midr) \
    (((midr) & MIDR_IMPLEMENTOR_MASK) >> MIDR_IMPLEMENTOR_SHIFT)

#define MIDR_CPU_MODEL(imp, partnum)            \
    (((imp)     << MIDR_IMPLEMENTOR_SHIFT) |    \
     (0xf       << MIDR_ARCHITECTURE_SHIFT) |   \
     ((partnum) << MIDR_PARTNUM_SHIFT))

#define MIDR_CPU_MODEL_MASK \
     (MIDR_IMPLEMENTOR_MASK | MIDR_PARTNUM_MASK | MIDR_ARCHITECTURE_MASK)

#define MIDR_IS_CPU_MODEL_RANGE(midr, model, rv_min, rv_max)            \
({                                                                      \
        u32 _model = (midr) & MIDR_CPU_MODEL_MASK;                      \
        u32 _rv = (midr) & (MIDR_REVISION_MASK | MIDR_VARIANT_MASK);    \
                                                                        \
        _model == (model) && _rv >= (rv_min) && _rv <= (rv_max);        \
})

#define ARM_CPU_IMP_ARM             0x41

#define ARM_CPU_PART_CORTEX_A12     0xC0D
#define ARM_CPU_PART_CORTEX_A17     0xC0E
#define ARM_CPU_PART_CORTEX_A15     0xC0F
#define ARM_CPU_PART_CORTEX_A53     0xD03
#define ARM_CPU_PART_CORTEX_A35     0xD04
#define ARM_CPU_PART_CORTEX_A55     0xD05
#define ARM_CPU_PART_CORTEX_A57     0xD07
#define ARM_CPU_PART_CORTEX_A72     0xD08
#define ARM_CPU_PART_CORTEX_A73     0xD09
#define ARM_CPU_PART_CORTEX_A75     0xD0A
#define ARM_CPU_PART_CORTEX_A76     0xD0B
#define ARM_CPU_PART_NEOVERSE_N1    0xD0C
#define ARM_CPU_PART_CORTEX_A77     0xD0D
#define ARM_CPU_PART_NEOVERSE_V1    0xD40
#define ARM_CPU_PART_CORTEX_A78     0xD41
#define ARM_CPU_PART_CORTEX_X1      0xD44
#define ARM_CPU_PART_CORTEX_A710    0xD47
#define ARM_CPU_PART_CORTEX_X2      0xD48
#define ARM_CPU_PART_NEOVERSE_N2    0xD49
#define ARM_CPU_PART_CORTEX_A78C    0xD4B

#define MIDR_CORTEX_A12 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A12)
#define MIDR_CORTEX_A17 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A17)
#define MIDR_CORTEX_A15 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A15)
#define MIDR_CORTEX_A53 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A53)
#define MIDR_CORTEX_A35 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A35)
#define MIDR_CORTEX_A55 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A55)
#define MIDR_CORTEX_A57 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A57)
#define MIDR_CORTEX_A72 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A72)
#define MIDR_CORTEX_A73 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A73)
#define MIDR_CORTEX_A75 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A75)
#define MIDR_CORTEX_A76 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A76)
#define MIDR_NEOVERSE_N1 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_NEOVERSE_N1)
#define MIDR_CORTEX_A77 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A77)
#define MIDR_NEOVERSE_V1 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_NEOVERSE_V1)
#define MIDR_CORTEX_A78 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A78)
#define MIDR_CORTEX_X1  MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_X1)
#define MIDR_CORTEX_A710 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A710)
#define MIDR_CORTEX_X2  MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_X2)
#define MIDR_NEOVERSE_N2 MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_NEOVERSE_N2)
#define MIDR_CORTEX_A78C MIDR_CPU_MODEL(ARM_CPU_IMP_ARM, ARM_CPU_PART_CORTEX_A78C)

/* MPIDR Multiprocessor Affinity Register */
#define _MPIDR_UP           (30)
#define MPIDR_UP            (1UL << _MPIDR_UP)
#define _MPIDR_SMP          (31)
#define MPIDR_SMP           (1UL << _MPIDR_SMP)
#define MPIDR_AFF0_SHIFT    (0)
#define MPIDR_AFF0_MASK     ((0xffUL) << MPIDR_AFF0_SHIFT)
#define MPIDR_HWID_MASK     (0xff00ffffffUL)

#define MPIDR_INVALID       (~MPIDR_HWID_MASK)
#define MPIDR_LEVEL_BITS    (8)


/*
 * Macros to extract affinity level (Aff0..Aff3 packed 8 bits apart in
 * the lower 32 bits of MPIDR; Aff3 lives in bits [39:32] for AArch64).
 */

#define MPIDR_LEVEL_BITS_SHIFT  3
#define MPIDR_LEVEL_MASK        ((1 << MPIDR_LEVEL_BITS) - 1)

#define MPIDR_LEVEL_SHIFT(level) \
         (((1 << (level)) >> 1) << MPIDR_LEVEL_BITS_SHIFT)

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
         (((mpidr) >> MPIDR_LEVEL_SHIFT(level)) & MPIDR_LEVEL_MASK)

#define AFFINITY_MASK(level)    ~(((0x1UL) << MPIDR_LEVEL_SHIFT(level)) - 1)

/* TTBCR Translation Table Base Control Register */
#define TTBCR_EAE    (0x80000000U)
#define TTBCR_N_MASK (0x07U)
#define TTBCR_N_16KB (0x00U)
#define TTBCR_N_8KB  (0x01U)
#define TTBCR_N_4KB  (0x02U)
#define TTBCR_N_2KB  (0x03U)
#define TTBCR_N_1KB  (0x04U)

/*
 * TTBCR_PD(0|1) can be applied only if LPAE is disabled, i.e., TTBCR.EAE==0
 * (ARM DDI 0487B.a G6-5203 and ARM DDI 0406C.b B4-1722).
 */
#define TTBCR_PD0       (1U << 4)
#define TTBCR_PD1       (1U << 5)

/* SCTLR System Control Register. */

/* Bits specific to SCTLR_EL1 for Arm32 */

#define SCTLR_A32_EL1_V     (1UL << 13)

/* Common bits for SCTLR_ELx for Arm32 */

#define SCTLR_A32_ELx_TE    (1UL << 30)
#define SCTLR_A32_ELx_FI    (1UL << 21)

/* Common bits for SCTLR_ELx for Arm64 */
#define SCTLR_A64_ELx_SA    (1UL << 3)

/* Common bits for SCTLR_ELx on all architectures */
#define SCTLR_Axx_ELx_EE    (1UL << 25)
#define SCTLR_Axx_ELx_WXN   (1UL << 19)
#define SCTLR_Axx_ELx_I     (1UL << 12)
#define SCTLR_Axx_ELx_C     (1UL << 2)
#define SCTLR_Axx_ELx_A     (1UL << 1)
#define SCTLR_Axx_ELx_M     (1UL << 0)


#define HSCTLR_RES1     ((1UL << 3) | (1UL << 4) | (1UL << 5) |\
                         (1UL << 6) | (1UL << 11) | (1UL << 16) |\
                         (1UL << 18) | (1UL << 22) | (1UL << 23) |\
                         (1UL << 28) | (1UL << 29))

#define HSCTLR_RES0     ((1UL << 7)  | (1UL << 8)  | (1UL << 9)  | (1UL << 10) |\
                         (1UL << 13) | (1UL << 14) | (1UL << 15) | (1UL << 17) |\
                         (1UL << 20) | (1UL << 24) | (1UL << 26) | (1UL << 27) |\
                         (1UL << 31))

/* Initial value for HSCTLR */
#define HSCTLR_SET      (HSCTLR_RES1    | SCTLR_Axx_ELx_A   | SCTLR_Axx_ELx_I)

/* Only used a pre-processing time... */
#define HSCTLR_CLEAR    (HSCTLR_RES0        | SCTLR_Axx_ELx_M   |\
                         SCTLR_Axx_ELx_C    | SCTLR_Axx_ELx_WXN |\
                         SCTLR_A32_ELx_FI   | SCTLR_Axx_ELx_EE  |\
                         SCTLR_A32_ELx_TE)




#define SCTLR_EL2_RES1  ((1UL << 4) | (1UL << 5) | (1UL << 11) |\
                         (1UL << 16) | (1UL << 18) | (1UL << 22) |\
                         (1UL << 23) | (1UL << 28) | (1UL << 29))

#define SCTLR_EL2_RES0  ((1UL << 6) | (1UL << 7) | (1UL << 8) |\
                         (1UL << 9) | (1UL << 10) | (1UL << 13) |\
                         (1UL << 14) | (1UL << 15) | (1UL << 17) |\
                         (1UL << 20) | (1UL << 21) | (1UL << 24) |\
                         (1UL << 26) | (1UL << 27) | (1UL << 30) |\
                         (1UL << 31) | (0xffffffffULL << 32))

/* Initial value for SCTLR_EL2 */
#define SCTLR_EL2_SET   (SCTLR_EL2_RES1     | SCTLR_A64_ELx_SA  |\
                         SCTLR_Axx_ELx_I)

/* Only used a pre-processing time... */
#define SCTLR_EL2_CLEAR (SCTLR_EL2_RES0     | SCTLR_Axx_ELx_M   |\
                         SCTLR_Axx_ELx_A    | SCTLR_Axx_ELx_C   |\
                         SCTLR_Axx_ELx_WXN  | SCTLR_Axx_ELx_EE)


/* HCR Hyp Configuration Register */
#define HCR_RW          (1UL << 31) /* Register Width, ARM64 only */
#define HCR_TGE         (1UL << 27) /* Trap General Exceptions */
#define HCR_TVM         (1UL << 26) /* Trap Virtual Memory Controls */
#define HCR_TTLB        (1UL << 25) /* Trap TLB Maintenance Operations */
#define HCR_TPU         (1UL << 24) /* Trap Cache Maintenance Operations to PoU */
#define HCR_TPC         (1UL << 23) /* Trap Cache Maintenance Operations to PoC */
#define HCR_TSW         (1UL << 22) /* Trap Set/Way Cache Maintenance Operations */
#define HCR_TAC         (1UL << 21) /* Trap ACTLR Accesses */
#define HCR_TIDCP       (1UL << 20) /* Trap lockdown */
#define HCR_TSC         (1UL << 19) /* Trap SMC instruction */
#define HCR_TID3        (1UL << 18) /* Trap ID Register Group 3 */
#define HCR_TID2        (1UL << 17) /* Trap ID Register Group 2 */
#define HCR_TID1        (1UL << 16) /* Trap ID Register Group 1 */
#define HCR_TID0        (1UL << 15) /* Trap ID Register Group 0 */
#define HCR_TWE         (1UL << 14) /* Trap WFE instruction */
#define HCR_TWI         (1UL << 13) /* Trap WFI instruction */
#define HCR_DC          (1UL << 12) /* Default cacheable */
#define HCR_BSU_MASK    (3UL << 10) /* Barrier Shareability Upgrade */
#define HCR_BSU_NONE     (0UL << 10)
#define HCR_BSU_INNER    (1UL << 10)
#define HCR_BSU_OUTER    (2UL << 10)
#define HCR_BSU_FULL     (3UL << 10)
#define HCR_FB          (1UL << 9) /* Force Broadcast of Cache/BP/TLB operations */
#define HCR_VA          (1UL << 8) /* Virtual Asynchronous Abort */
#define HCR_VI          (1UL << 7) /* Virtual IRQ */
#define HCR_VF          (1UL << 6) /* Virtual FIQ */
#define HCR_AMO         (1UL << 5) /* Override CPSR.A */
#define HCR_IMO         (1UL << 4) /* Override CPSR.I */
#define HCR_FMO         (1UL << 3) /* Override CPSR.F */
#define HCR_PTW         (1UL << 2) /* Protected Walk */
#define HCR_SWIO        (1UL << 1) /* Set/Way Invalidation Override */
#define HCR_VM          (1UL << 0) /* Virtual MMU Enable */

/* TCR: Stage 1 Translation Control */
/* See Arm® Architecture Reference Manual, ARM DDI 0487E.a */
#define TCR_T0SZ_SHIFT  (0)
#define TCR_T1SZ_SHIFT  (16)
#define TCR_T0SZ(x)     ((x)<<TCR_T0SZ_SHIFT)

/*
 * According to ARM DDI 0487B.a, TCR_EL1.{T0SZ,T1SZ} (AArch64, page D7-2480)
 * comprises 6 bits and TTBCR.{T0SZ,T1SZ} (AArch32, page G6-5204) comprises 3
 * bits following another 3 bits for RES0. Thus, the mask for both registers
 * should be 0x3f.
 */
#define TCR_SZ_MASK     ((0x3fUL))

#define TCR_EPD0        (0x1UL << 7)
#define TCR_EPD1        (0x1UL << 23)

#define TCR_IRGN0_NC    (0x0UL << 8)
#define TCR_IRGN0_WBWA  (0x1UL << 8)
#define TCR_IRGN0_WT    (0x2UL << 8)
#define TCR_IRGN0_WB    (0x3UL << 8)

#define TCR_ORGN0_NC    (0x0UL << 10)
#define TCR_ORGN0_WBWA  (0x1UL << 10)
#define TCR_ORGN0_WT    (0x2UL << 10)
#define TCR_ORGN0_WB    (0x3UL << 10)

#define TCR_SH0_NS      (0x0UL << 12)
#define TCR_SH0_OS      (0x2UL << 12)
#define TCR_SH0_IS      (0x3UL << 12)

/* Note that the fields TCR_EL1.{TG0,TG1} are not available on AArch32. */
#define TCR_TG0_SHIFT   (14)
#define TCR_TG0_MASK    (0x3UL << TCR_TG0_SHIFT)
#define TCR_TG0_4K      (0x0UL << TCR_TG0_SHIFT)
#define TCR_TG0_64K     (0x1UL << TCR_TG0_SHIFT)
#define TCR_TG0_16K     (0x2UL << TCR_TG0_SHIFT)

/* Note that the field TCR_EL2.TG1 exists only if HCR_EL2.E2H==1. */
#define TCR_EL1_TG1_SHIFT   (30)
#define TCR_EL1_TG1_MASK    (0x3UL << TCR_EL1_TG1_SHIFT)
#define TCR_EL1_TG1_16K     (0x1UL << TCR_EL1_TG1_SHIFT)
#define TCR_EL1_TG1_4K      (0x2UL << TCR_EL1_TG1_SHIFT)
#define TCR_EL1_TG1_64K     (0x3UL << TCR_EL1_TG1_SHIFT)

/*
 * Note that the field TCR_EL1.IPS is not available on AArch32. Also, the field
 * TCR_EL2.IPS exists only if HCR_EL2.E2H==1.
 */
#define TCR_EL1_IPS_SHIFT   (32)
#define TCR_EL1_IPS_MASK    (0x7UL << TCR_EL1_IPS_SHIFT)
#define TCR_EL1_IPS_32_BIT  (0x0UL << TCR_EL1_IPS_SHIFT)
#define TCR_EL1_IPS_36_BIT  (0x1UL << TCR_EL1_IPS_SHIFT)
#define TCR_EL1_IPS_40_BIT  (0x2UL << TCR_EL1_IPS_SHIFT)
#define TCR_EL1_IPS_42_BIT  (0x3UL << TCR_EL1_IPS_SHIFT)
#define TCR_EL1_IPS_44_BIT  (0x4UL << TCR_EL1_IPS_SHIFT)
#define TCR_EL1_IPS_48_BIT  (0x5UL << TCR_EL1_IPS_SHIFT)
#define TCR_EL1_IPS_52_BIT  (0x6UL << TCR_EL1_IPS_SHIFT)

/*
 * The following values correspond to the bit masks represented by
 * TCR_EL1_IPS_XX_BIT defines.
 */
#define TCR_EL1_IPS_32_BIT_VAL  (32)
#define TCR_EL1_IPS_36_BIT_VAL  (36)
#define TCR_EL1_IPS_40_BIT_VAL  (40)
#define TCR_EL1_IPS_42_BIT_VAL  (42)
#define TCR_EL1_IPS_44_BIT_VAL  (44)
#define TCR_EL1_IPS_48_BIT_VAL  (48)
#define TCR_EL1_IPS_52_BIT_VAL  (52)
#define TCR_EL1_IPS_MIN_VAL     (25)

/* Note that the fields TCR_EL2.TBI(0|1) exist only if HCR_EL2.E2H==1. */
#define TCR_EL1_TBI0    (0x1UL << 37)
#define TCR_EL1_TBI1    (0x1UL << 38)


#define TCR_PS(x)       ((x)<<16)
#define TCR_TBI         (0x1UL << 20)

#define TCR_RES1        (1UL << 31|(1UL)<<23)


/* VTCR: Stage 2 Translation Control */

#define VTCR_T0SZ(x)    ((x)<<0)

#define VTCR_SL0(x)     ((x)<<6)

#define VTCR_IRGN0_NC   (0x0UL << 8)
#define VTCR_IRGN0_WBWA (0x1UL << 8)
#define VTCR_IRGN0_WT   (0x2UL << 8)
#define VTCR_IRGN0_WB   (0x3UL << 8)

#define VTCR_ORGN0_NC   (0x0UL << 10)
#define VTCR_ORGN0_WBWA (0x1UL << 10)
#define VTCR_ORGN0_WT   (0x2UL << 10)
#define VTCR_ORGN0_WB   (0x3UL << 10)

#define VTCR_SH0_NS     (0x0UL << 12)
#define VTCR_SH0_OS     (0x2UL << 12)
#define VTCR_SH0_IS     (0x3UL << 12)


#define VTCR_TG0_4K     (0x0UL << 14)
#define VTCR_TG0_64K    (0x1UL << 14)
#define VTCR_TG0_16K    (0x2UL << 14)

#define VTCR_PS(x)      ((x)<<16)

#define VTCR_VS    	    (0x1UL << 19)


#define VTCR_RES1       (1UL << 31)

/* HCPTR Hyp. Coprocessor Trap Register */
#define HCPTR_TAM       ((1U << 30))
#define HCPTR_TTA       ((1U << 20))        /* Trap trace registers */
#define HCPTR_CP(x)     ((1U << (x)))       /* Trap Coprocessor x */
#define HCPTR_CP_MASK   ((1U << 14)-1)

/* HSTR Hyp. System Trap Register */
#define HSTR_T(x)       ((1U << (x)))       /* Trap Cp15 c<x> */

/* HDCR Hyp. Debug Configuration Register */
#define HDCR_TDRA       (1U << 11)          /* Trap Debug ROM access */
#define HDCR_TDOSA      (1U << 10)          /* Trap Debug-OS-related register access */
#define HDCR_TDA        (1U << 9)           /* Trap Debug Access */
#define HDCR_TDE        (1U << 8)           /* Route Soft Debug exceptions from EL1/EL1 to EL2 */
#define HDCR_TPM        (1U << 6)           /* Trap Performance Monitors accesses */
#define HDCR_TPMCR      (1U << 5)           /* Trap PMCR accesses */

#define HSR_EC_SHIFT                26

#define HSR_EC_UNKNOWN              0x00
#define HSR_EC_WFI_WFE              0x01
#define HSR_EC_CP15_32              0x03
#define HSR_EC_CP15_64              0x04
#define HSR_EC_CP14_32              0x05        /* Trapped MCR or MRC access to CP14 */
#define HSR_EC_CP14_DBG             0x06        /* Trapped LDC/STC access to CP14 (only for debug registers) */
#define HSR_EC_CP                   0x07        /* HCPTR-trapped access to CP0-CP13 */
#define HSR_EC_CP10                 0x08
#define HSR_EC_JAZELLE              0x09
#define HSR_EC_BXJ                  0x0a
#define HSR_EC_CP14_64              0x0c
#define HSR_EC_SVC32                0x11
#define HSR_EC_HVC32                0x12
#define HSR_EC_SMC32                0x13

#define HSR_EC_SVC64                0x15
#define HSR_EC_HVC64                0x16
#define HSR_EC_SMC64                0x17
#define HSR_EC_SYSREG               0x18
#define HSR_EC_SVE                  0x19

#define HSR_EC_INSTR_ABORT_LOWER_EL 0x20
#define HSR_EC_INSTR_ABORT_CURR_EL  0x21
#define HSR_EC_DATA_ABORT_LOWER_EL  0x24
#define HSR_EC_DATA_ABORT_CURR_EL   0x25

#define HSR_EC_BRK                  0x3c



/* FSR format, common */
#define FSR_LPAE                (1UL << 9)
/* FSR short format */
#define FSRS_FS_DEBUG           (0UL << 10|(0x2UL)<<0)
/* FSR long format */
#define FSRL_STATUS_DEBUG       (0x22UL << 0)


#define MM64_VMID_8_BITS_SUPPORT    0x0
#define MM64_VMID_16_BITS_SUPPORT   0x2



/* Physical Address Register */
#define PAR_F           (1U << 0)

/* .... If F == 1 */
#define PAR_FSC_SHIFT   (1)
#define PAR_FSC_MASK    (0x3fU << PAR_FSC_SHIFT)
#define PAR_STAGE21     (1U << 8)     /* Stage 2 Fault During Stage 1 Walk */
#define PAR_STAGE2      (1U << 9)     /* Stage 2 Fault */

/* If F == 0 */
#define PAR_MAIR_SHIFT  56                       /* Memory Attributes */
#define PAR_MAIR_MASK   (0xffLL<<PAR_MAIR_SHIFT)
#define PAR_NS          (1U << 9)                   /* Non-Secure */
#define PAR_SH_SHIFT    7                        /* Shareability */
#define PAR_SH_MASK     (3U << PAR_SH_SHIFT)

/* Fault Status Register */
/*
 * 543210 BIT
 * 00XXLL -- XX Fault Level LL
 * ..01LL -- Translation Fault LL
 * ..10LL -- Access Fault LL
 * ..11LL -- Permission Fault LL
 * 01xxxx -- Abort/Parity
 * 10xxxx -- Other
 * 11xxxx -- Implementation Defined
 */
#define FSC_TYPE_MASK (0x3U << 4)
#define FSC_TYPE_FAULT (0x00U << 4)
#define FSC_TYPE_ABT   (0x01U << 4)
#define FSC_TYPE_OTH   (0x02U << 4)
#define FSC_TYPE_IMPL  (0x03U << 4)

#define FSC_FLT_TRANS  (0x04)
#define FSC_FLT_ACCESS (0x08)
#define FSC_FLT_PERM   (0x0c)
#define FSC_SEA        (0x10) /* Synchronous External Abort */
#define FSC_SPE        (0x18) /* Memory Access Synchronous Parity Error */
#define FSC_APE        (0x11) /* Memory Access Asynchronous Parity Error */
#define FSC_SEATT      (0x14) /* Sync. Ext. Abort Translation Table */
#define FSC_SPETT      (0x1c) /* Sync. Parity. Error Translation Table */
#define FSC_AF         (0x21) /* Alignment Fault */
#define FSC_DE         (0x22) /* Debug Event */
#define FSC_TLB_FLT    (0x30) /* TLB conflict abort */
#define FSC_UNS_STOMIC (0x31) /* Unsupported atomic hardware update fault */
#define FSC_LKD        (0x34) /* Lockdown Abort */
#define FSC_UNS_EXCL   (0x35) /* Unsupported Exclusive or Atomic access) */
#define FSC_CPR        (0x3a) /* Coprocossor Abort */

#define FSC_LL_MASK    (0x03U << 0)

/* HPFAR_EL2: Hypervisor IPA Fault Address Register */
#define HPFAR_MASK	GENMASK(39, 4)


/* Time counter hypervisor control register */
#define CNTHCTL_EL2_EL1PCTEN (1u<<0) /* Kernel/user access to physical counter */
#define CNTHCTL_EL2_EL1PCEN  (1u<<1) /* Kernel/user access to CNTP timer regs */

/* Time counter kernel control register */
#define CNTKCTL_EL1_EL0PCTEN (1u<<0) /* Expose phys counters to EL0 */
#define CNTKCTL_EL1_EL0VCTEN (1u<<1) /* Expose virt counters to EL0 */
#define CNTKCTL_EL1_EL0VTEN  (1u<<8) /* Expose virt timer registers to EL0 */
#define CNTKCTL_EL1_EL0PTEN  (1u<<9) /* Expose phys timer registers to EL0 */

/* Timer control registers */
#define CNTx_CTL_ENABLE   (1UL<<0)  /* Enable timer */
#define CNTx_CTL_MASK     (1UL<<1)  /* Mask IRQ */
#define CNTx_CTL_PENDING  (1UL<<2)  /* IRQ pending */

/* Timer frequency mask */
#define CNTFRQ_MASK       GENMASK(31, 0)

/* Exception Vector offsets */
/* ... ARM32 */
#define VECTOR32_RST  0
#define VECTOR32_UND  4
#define VECTOR32_SVC  8
#define VECTOR32_PABT 12
#define VECTOR32_DABT 16
/* ... ARM64 */
#define VECTOR64_CURRENT_SP0_BASE  0x000
#define VECTOR64_CURRENT_SPx_BASE  0x200
#define VECTOR64_LOWER64_BASE      0x400
#define VECTOR64_LOWER32_BASE      0x600

#define VECTOR64_SYNC_OFFSET       0x000
#define VECTOR64_IRQ_OFFSET        0x080
#define VECTOR64_FIQ_OFFSET        0x100
#define VECTOR64_ERROR_OFFSET      0x180

#define PSR_MODE64_MASK				0x0000000f
#define PSR_MODE64_EL0t				0x00000000
#define PSR_MODE64_EL1t				0x00000004
#define PSR_MODE64_EL1h				0x00000005
#define PSR_MODE64_EL2t				0x00000008
#define PSR_MODE64_EL2h				0x00000009
#define PSR_MODE64_EL3t				0x0000000c
#define PSR_MODE64_EL3h				0x0000000d