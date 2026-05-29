/*
 * AArch64 EL3 / EL2 / EL1 control-register bitfield definitions.
 *
 * Almost every constant here is a single bit position taken straight
 * from the ARM Architecture Reference Manual (DDI 0487).  The macros
 * are arranged by owning register so that callers can include the
 * file once and pick the bits they need.
 *
 * Naming convention:
 *
 *   <REG>_<FIELD>_<ROLE>
 *
 * where ROLE is one of:
 *   EN   - the bit must be set to enable the feature
 *   DIS  - the bit must be cleared to disable the feature; the macro
 *          expands to 0 so that "all DIS bits ORed" still equals 0
 *          (kept for backwards compatibility with the existing call
 *          sites that ORed every DIS bit explicitly)
 *   MASK - non-zero mask used to read the field out of the register
 *   RES1 - architecturally-reserved bits that must read as one
 *
 * A handful of pre-AArch64 SCTLR shorthand bits (CR_M/CR_A/...) are
 * also kept around for the early start-up code that does not yet use
 * the EL-specific names.
 */

#pragma once


/* =================================================================
 * Generic SCTLR bits (pre-EL split, used by early boot code).
 * ================================================================= */

#define CR_M    (1u << 0)   /* MMU enable                              */
#define CR_A    (1u << 1)   /* Alignment fault check enable            */
#define CR_C    (1u << 2)   /* Data-cache enable                       */
#define CR_SA   (1u << 3)   /* Stack alignment check enable            */
#define CR_I    (1u << 12)  /* Instruction-cache enable                */
#define CR_WXN  (1u << 19)  /* Writeable implies execute-never         */
#define CR_EE   (1u << 25)  /* Exception endianness (BE if set)        */

/* SCR_EL3.RW selector convenience macros. */
#define ES_TO_AARCH64  1
#define ES_TO_AARCH32  0


/* =================================================================
 * SCR_EL3 (Secure Configuration Register, EL3 only).
 * ================================================================= */

#define SCR_EL3_RW_AARCH64  (1u << 10)  /* Next lower EL runs AArch64  */
#define SCR_EL3_RW_AARCH32  (0u << 10)  /* Next lower EL runs AArch32  */
#define SCR_EL3_HCE_EN      (1u <<  8)  /* HVC instruction enabled     */
#define SCR_EL3_SMD_DIS     (1u <<  7)  /* SMC instruction disabled    */
#define SCR_EL3_RES1        (3u <<  4)  /* RES1                        */
#define SCR_EL3_EA_EN       (1u <<  3)  /* SErrors routed to EL3       */
#define SCR_EL3_NS_EN       (1u <<  0)  /* EL0/EL1 are Non-secure      */


/* =================================================================
 * SPSR_EL[123] (saved program status, common bit layout).
 * ================================================================= */

#define SPSR_EL_END_LE       (0u << 9)  /* Exception endian = LE       */
#define SPSR_EL_DEBUG_MASK   (1u << 9)  /* Mask debug exceptions       */
#define SPSR_EL_ASYN_MASK    (1u << 8)  /* Mask async aborts (legacy)  */
#define SPSR_EL_SERR_MASK    (1u << 8)  /* Mask SError                 */
#define SPSR_EL_IRQ_MASK     (1u << 7)  /* Mask IRQ                    */
#define SPSR_EL_FIQ_MASK     (1u << 6)  /* Mask FIQ                    */

#define SPSR_EL_T_A32        (0u << 5)  /* Came from A32 ISA           */
#define SPSR_EL_M_AARCH64    (0u << 4)  /* Came from AArch64           */
#define SPSR_EL_M_AARCH32    (1u << 4)  /* Came from AArch32           */

/* SPSR.M low bits = source mode encoding. */
#define SPSR_EL_M_SVC        0x3u       /* From SVC mode               */
#define SPSR_EL_M_HYP        0xau       /* From HYP mode               */
#define SPSR_EL_M_EL1H       0x5u       /* From EL1h                   */
#define SPSR_EL_M_EL2T       0x8u       /* From EL2t                   */
#define SPSR_EL_M_EL2H       0x9u       /* From EL2h                   */


/* =================================================================
 * CPTR_EL2 (coprocessor trap controls, EL2).
 *
 * RES1 keeps bits [13:12] and [9:0] set; the rest are managed by
 * the boot code on a per-feature basis.
 * ================================================================= */

#define CPTR_EL2_RES1   ((3u << 12) | 0x3ffu)


/* =================================================================
 * CNTHCTL_EL2 (hypervisor timer control).
 * ================================================================= */

#define CNTHCTL_EL2_EL1PCEN_EN  (1u << 1)  /* EL1 phys-timer accessible */
#define CNTHCTL_EL2_EL1PCTEN_EN (1u << 0)  /* EL1 phys-counter readable */


/* =================================================================
 * HCR_EL2 (hypervisor configuration, partial).
 *
 * Only the bits actively touched by the boot path live here; the
 * comprehensive table is in aarch64_hcr.h.
 * ================================================================= */

#define HCR_EL2_API         (1ull << 41) /* Trap PtrAuth instructions  */
#define HCR_EL2_APK         (1ull << 40) /* Trap PtrAuth key access    */
#define HCR_EL2_RW_AARCH64  (1ull << 31) /* EL1 runs AArch64           */
#define HCR_EL2_RW_AARCH32  (0ull << 31) /* EL1 runs AArch32           */
#define HCR_EL2_HCD_DIS     (1ull << 29) /* HVC disabled               */
#define HCR_EL2_AMO_EL2     (1ull <<  5) /* Route SErrors to EL2       */


/* =================================================================
 * ID_AA64ISAR0/1_EL1 feature-detection masks.
 * ================================================================= */

#define ID_AA64ISAR0_EL1_RNDR  (0xFULL << 60) /* RNDR/RNDRRS available  */

#define ID_AA64ISAR1_EL1_GPI   (0xFu   << 28) /* GP impl-defined algo  */
#define ID_AA64ISAR1_EL1_GPA   (0xFu   << 24) /* GP QARMA algo         */
#define ID_AA64ISAR1_EL1_API   (0xFu   <<  8) /* API impl-defined algo */
#define ID_AA64ISAR1_EL1_APA   (0xFu   <<  4) /* API QARMA algo        */


/* =================================================================
 * ID_AA64PFR0_EL1 feature-detection masks (subset).
 * ================================================================= */

#define ID_AA64PFR0_EL1_EL3    (0xFu << 12)   /* EL3 implementation    */
#define ID_AA64PFR0_EL1_EL2    (0xFu <<  8)   /* EL2 implementation    */


/* =================================================================
 * CPACR_EL1 (coprocessor access, EL1).
 * ================================================================= */

#define CPACR_EL1_FPEN_EN   (3u << 20)  /* SIMD/FP access enabled      */


/* =================================================================
 * SCTLR_EL1 (system control, EL1).
 *
 * RES1 captures the four bit positions that the architecture
 * requires to read back as one (28, 29, 22, 23, 20, 11).
 *
 * The _DIS macros below all expand to 0; they exist so the boot
 * code can document, by name, which bits it is intentionally
 * leaving cleared when it builds a final SCTLR_EL1 value.
 * ================================================================= */

#define SCTLR_EL1_RES1          ((3u << 28) | (3u << 22) | (1u << 20) | (1u << 11))

#define SCTLR_EL1_UCI_DIS       (0u << 26)  /* Cache instructions disabled    */
#define SCTLR_EL1_EE_LE         (0u << 25)  /* Little-endian exceptions       */
#define SCTLR_EL1_WXN_DIS       (0u << 19)  /* W != XN                        */
#define SCTLR_EL1_NTWE_DIS      (0u << 18)  /* WFE not trapped                */
#define SCTLR_EL1_NTWI_DIS      (0u << 16)  /* WFI not trapped                */
#define SCTLR_EL1_UCT_DIS       (0u << 15)  /* CTR_EL0 access disabled        */
#define SCTLR_EL1_DZE_DIS       (0u << 14)  /* DC ZVA disabled                */
#define SCTLR_EL1_ICACHE_DIS    (0u << 12)  /* I-cache disabled               */
#define SCTLR_EL1_UMA_DIS       (0u <<  9)  /* User mask access disabled      */
#define SCTLR_EL1_SED_EN        (0u <<  8)  /* SETEND enabled                 */
#define SCTLR_EL1_ITD_EN        (0u <<  7)  /* IT enabled                     */
#define SCTLR_EL1_CP15BEN_DIS   (0u <<  5)  /* CP15 barrier disabled          */
#define SCTLR_EL1_SA0_DIS       (0u <<  4)  /* EL0 SP alignment check off     */
#define SCTLR_EL1_SA_DIS        (0u <<  3)  /* EL1 SP alignment check off     */
#define SCTLR_EL1_DCACHE_DIS    (0u <<  2)  /* D-cache disabled               */
#define SCTLR_EL1_ALIGN_DIS     (0u <<  1)  /* Alignment check disabled       */
#define SCTLR_EL1_MMU_DIS       (0u)        /* MMU disabled                   */


/* =================================================================
 * SCTLR_EL2 (system control, EL2).
 * See aarch64_hcr.h for SCTLR_EL2_RES1 / RES0 / SET definitions.
 * ================================================================= */

#define SCTLR_EL2_EE_LE         (0u << 25)  /* Little-endian exceptions       */
#define SCTLR_EL2_WXN_DIS       (0u << 19)  /* W != XN                        */
#define SCTLR_EL2_ICACHE_DIS    (0u << 12)  /* I-cache disabled               */
#define SCTLR_EL2_SA_DIS        (0u <<  3)  /* SP alignment check off         */
#define SCTLR_EL2_DCACHE_DIS    (0u <<  2)  /* D-cache disabled               */
#define SCTLR_EL2_ALIGN_DIS     (0u <<  1)  /* Alignment check disabled       */
#define SCTLR_EL2_MMU_DIS       (0u)        /* MMU disabled                   */
