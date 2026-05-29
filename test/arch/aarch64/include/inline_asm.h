/*
 * Frequently used AArch64 inline-assembly primitives for HyperCoreRT.
 *
 * Everything here is a thin wrapper around a single AArch64 instruction
 * (or a handful of related ones) plus a memory clobber where required.
 * Macros are preferred over inline functions so that the asm operand
 * registers are inferred directly at the call site.
 *
 * Grouping inside the file:
 *   - Byte-swap helpers (REV16/REV32/REV64)
 *   - Load/store exclusive primitives (LDXR/STXR/CLREX)
 *   - System register access (MRS/MSR family)
 *   - TLB maintenance (TLBI family)
 *   - Address translation (AT family)
 *   - CPU feature probes (ID_AA64* readers)
 *   - Exception-level helpers (CurrentEL, SCTLR, ...)
 *   - Miscellaneous (WFI, per-CPU identifiers)
 */

#pragma once

#include <sys/types.h>


/* ------------------------------------------------------------------ */
/* Byte-swap helpers                                                  */
/* ------------------------------------------------------------------ */

#define rev16(_val)                                                   \
    ({                                                                \
        u16 _ret;                                                     \
        asm volatile ("rev16 %0, %1"                                  \
                      : "=r"(_ret)                                    \
                      : "r"(_val)                                     \
                      : "memory", "cc");                              \
        _ret;                                                         \
    })

#define rev32(_val)                                                   \
    ({                                                                \
        u32 _ret;                                                     \
        asm volatile ("rev32 %0, %1"                                  \
                      : "=r"(_ret)                                    \
                      : "r"(_val)                                     \
                      : "memory", "cc");                              \
        _ret;                                                         \
    })

/*
 * AArch64 has a single REV instruction that operates on the full
 * 64-bit register; we expose it as rev64 for symmetry with the
 * 16/32-bit helpers.  We split into two 32-bit halves and reuse
 * rev32 to keep the macro available even when the compiler chooses
 * to inline it in pieces.
 */
#define rev64(_val)                                                   \
    ({                                                                \
        u32 _hi = (u32)((uint64_t)(_val) >> 32);                      \
        u32 _lo = (u32)(_val);                                        \
        _hi = rev32(_hi);                                             \
        _lo = rev32(_lo);                                             \
        ((uint64_t)_lo << 32) | (uint64_t)_hi;                        \
    })


/* ------------------------------------------------------------------ */
/* Load/store exclusive primitives                                    */
/* ------------------------------------------------------------------ */

#define ldxr(_addr, _out)                                             \
    asm volatile ("ldxr %0, [%1]"                                     \
                  : "=r"(_out)                                        \
                  : "r"(_addr))

#define stxr(_addr, _in, _status)                                     \
    asm volatile ("stxr %0, %1, [%2]"                                 \
                  : "=r"(_status)                                     \
                  : "r"(_in), "r"(_addr))

#define clrex() asm volatile ("clrex")


/* ------------------------------------------------------------------ */
/* System register access                                             */
/*                                                                    */
/* read_sysreg/write_sysreg use the encoded MRS_S/MSR_S forms so      */
/* that registers without a mnemonic (e.g. ICH_*_EL2) can be          */
/* accessed by symbolic name; mrs/msr/msr_sync are the plain forms    */
/* used for ARM-architected registers.                                */
/* ------------------------------------------------------------------ */

#define read_sysreg(_reg)                                             \
    ({                                                                \
        uint64_t _v;                                                  \
        asm volatile ("mrs_s %0, " stringify(_reg)                    \
                      : "=r"(_v));                                    \
        _v;                                                           \
    })

#define write_sysreg(_v, _reg)                                        \
    do {                                                              \
        asm volatile ("msr " stringify(_reg) ", %0\n\t"               \
                      "dsb sy\n\t"                                    \
                      "isb\n\t"                                       \
                      :                                               \
                      : "r"((uint64_t)(_v)));                         \
    } while (0)

#define mrs(_reg)                                                     \
    ({                                                                \
        uint64_t _v;                                                  \
        asm volatile ("mrs %0, " stringify(_reg)                      \
                      : "=r"(_v));                                    \
        _v;                                                           \
    })

#define msr(_reg, _v)                                                 \
    do {                                                              \
        asm volatile ("msr " stringify(_reg) ", %0"                   \
                      :                                               \
                      : "r"(_v));                                     \
    } while (0)

/* Same as msr() but adds a dsb-sy / isb pair to force the write to
 * take effect before any subsequent instructions are fetched. */
#define msr_sync(_reg, _v)                                            \
    do {                                                              \
        asm volatile ("msr " stringify(_reg) ", %0\n\t"               \
                      "dsb sy\n\t"                                    \
                      "isb\n\t"                                       \
                      :                                               \
                      : "r"(_v));                                     \
    } while (0)

#define read_mpidr() mrs(mpidr_el1)


/* ------------------------------------------------------------------ */
/* TLB maintenance                                                    */
/*                                                                    */
/* Each helper issues the TLBI variant followed by a dsb-ish/isb pair */
/* so that the invalidation is observed by every PE in the inner-     */
/* shareable domain before the next instruction is fetched.           */
/* ------------------------------------------------------------------ */

#define __tlb_op_barrier()                                            \
    "dsb ish\n\t"                                                     \
    "isb\n\t"

/* Invalidate all EL2 TLB entries on all inner-shareable PEs. */
#define tlb_inv_hyp_all()                                             \
    asm volatile ("tlbi alle2is\n\t"                                  \
                  __tlb_op_barrier()                                  \
                  ::: "memory", "cc")

/* Invalidate all stage-1 EL1 TLB entries (current VMID) on all
 * inner-shareable PEs. */
#define tlb_inv_guest_allis()                                         \
    asm volatile ("tlbi alle1is\n\t"                                  \
                  __tlb_op_barrier()                                  \
                  ::: "memory", "cc")

/* Invalidate all stage 1+2 TLB entries for the current VMID. */
#define tlb_inv_guest_cur()                                           \
    asm volatile ("tlbi vmalls12e1is\n\t"                             \
                  __tlb_op_barrier()                                  \
                  ::: "memory", "cc")

/* Invalidate EL2 TLB entry that translates a single hypervisor VA. */
#define tlb_inv_hyp_vais(_va)                                         \
    asm volatile ("tlbi vae2is, %0\n\t"                               \
                  __tlb_op_barrier()                                  \
                  :                                                   \
                  : "r"((_va) >> 12)                                  \
                  : "memory", "cc")

/* Invalidate stage-2 IPA mapping at the given guest physical address. */
#define tlb_inv_guest_ipa(_ipa)                                       \
    asm volatile ("tlbi ipas2e1is, %0\n\t"                            \
                  __tlb_op_barrier()                                  \
                  :                                                   \
                  : "r"((_ipa) >> 12)                                 \
                  : "memory", "cc")

/* Invalidate stage-1 EL1 entry by VA for any ASID. */
#define tlb_inv_guest_va(_va)                                         \
    asm volatile ("tlbi vaae1is, %0\n\t"                              \
                  __tlb_op_barrier()                                  \
                  :                                                   \
                  : "r"((_va) >> 12)                                  \
                  : "memory", "cc")


/* ------------------------------------------------------------------ */
/* Address translation (AT family)                                    */
/* ------------------------------------------------------------------ */

#define VA2PA_STAGE1   "s1"
#define VA2PA_STAGE12  "s12"
#define VA2PA_EL0      "e0"
#define VA2PA_EL1      "e1"
#define VA2PA_EL2      "e2"
#define VA2PA_EL3      "e3"
#define VA2PA_RD       "r"
#define VA2PA_WR       "w"

#define va2pa_at(_stage, _el, _rw, _va)                               \
    asm volatile ("at " _stage _el _rw ", %0"                         \
                  :                                                   \
                  : "r"(_va)                                          \
                  : "memory", "cc")


/* ------------------------------------------------------------------ */
/* CPU feature probes                                                 */
/*                                                                    */
/* All probes read a single ID register and mask the relevant field;  */
/* callers receive the raw masked value so they can compare against   */
/* the expected encoding (typically non-zero == supported).           */
/* ------------------------------------------------------------------ */

#define __cpu_id_field(_reg, _mask)                                   \
    ({                                                                \
        uint64_t _idr;                                                \
        asm volatile ("mrs %0, " #_reg : "=r"(_idr));                 \
        (_idr & (_mask));                                             \
    })

/* Pointer authentication */
#define cpu_has_address_auth_arch()                                   \
    __cpu_id_field(id_aa64isar1_el1, ID_AA64ISAR1_APA_MASK)
#define cpu_has_address_auth_imp()                                    \
    __cpu_id_field(id_aa64isar1_el1, ID_AA64ISAR1_API_MASK)

/* 32-bit-mode features advertised by ID_PFR0_EL1 */
#define cpu_has_thumbee()  __cpu_id_field(id_pfr0_el1, ID_PFR0_THUMBEE_MASK)
#define cpu_has_thumb()    __cpu_id_field(id_pfr0_el1, ID_PFR0_THUMBEE_MASK)
#define cpu_has_jazelle()  __cpu_id_field(id_pfr0_el1, ID_PFR0_JAZELLE_MASK)
#define cpu_has_arm()      __cpu_id_field(id_pfr0_el1, ID_PFR0_ARM_MASK)

/* Note: this is the original "Thumb-2 advertised" check; the result
 * is non-zero only when the PFR0 Thumb field equals the Thumb-2 code. */
#define cpu_has_thumb2()                                              \
    (__cpu_id_field(id_pfr0_el1, ID_PFR0_THUMB_MASK) == ID_PFR0_THUMB2_MASK)

/* 64-bit features advertised by ID_AA64PFR0_EL1 */
#define cpu_has_asimd()                                               \
    (__cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_ASIMD_MASK) != 0xf)
#define cpu_has_fpu()                                                 \
    (__cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_FPU_MASK) != 0xf)

/* AArch32-at-EL{0..3} checks: true iff the EL field matches the A32 code. */
#define cpu_has_el0_a32()                                             \
    (__cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL0_MASK) == ID_AA64PFR0_EL0_A32)
#define cpu_has_el1_a32()                                             \
    (__cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL1_MASK) == ID_AA64PFR0_EL1_A32)
#define cpu_has_el2_a32()                                             \
    (__cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL2_MASK) == ID_AA64PFR0_EL2_A32)
#define cpu_has_el3_a32()                                             \
    (__cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL3_MASK) == ID_AA64PFR0_EL3_A32)

/* EL implemented at all? (non-zero == yes) */
#define cpu_has_el0() __cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL0_MASK)
#define cpu_has_el1() __cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL1_MASK)
#define cpu_has_el2() __cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL2_MASK)
#define cpu_has_el3() __cpu_id_field(id_aa64pfr0_el1, ID_AA64PFR0_EL3_MASK)


/* ------------------------------------------------------------------ */
/* Miscellaneous                                                      */
/* ------------------------------------------------------------------ */

#define wfi() asm volatile ("wfi" : : : "memory")


/* ------------------------------------------------------------------ */
/* Exception level helpers                                            */
/* ------------------------------------------------------------------ */

static inline unsigned int current_el(void)
{
    unsigned long raw;

    asm volatile ("mrs %0, CurrentEL" : "=r"(raw) : : "cc");
    /* Bits [3:2] hold the EL, the rest are RES0. */
    return (unsigned int)((raw >> 2) & 0x3);
}

static inline unsigned long get_sctlr(void)
{
    unsigned long val;

    switch (current_el()) {
    case 1: asm volatile ("mrs %0, sctlr_el1" : "=r"(val) : : "cc"); break;
    case 2: asm volatile ("mrs %0, sctlr_el2" : "=r"(val) : : "cc"); break;
    default:
        /* EL3 (and any unexpected value) falls through here. */
        asm volatile ("mrs %0, sctlr_el3" : "=r"(val) : : "cc"); break;
    }
    return val;
}

static inline void set_sctlr(unsigned long val)
{
    switch (current_el()) {
    case 1: asm volatile ("msr sctlr_el1, %0" : : "r"(val) : "cc"); break;
    case 2: asm volatile ("msr sctlr_el2, %0" : : "r"(val) : "cc"); break;
    default:
        asm volatile ("msr sctlr_el3, %0" : : "r"(val) : "cc"); break;
    }
    asm volatile ("isb");
}


/* ------------------------------------------------------------------ */
/* Per-CPU identifiers                                                */
/* ------------------------------------------------------------------ */

static inline uint64_t thread_id(void)
{
    unsigned long val;
    asm volatile ("mrs %0, tpidr_el2" : "=r"(val));
    return val;
}

/* MPIDR with the U/MT/RES bits masked off so callers can hash on
 * the affinity fields alone. */
static inline uint64_t smp_id(void)
{
    unsigned long val;
    asm volatile ("mrs %0, mpidr_el1" : "=r"(val));
    return val & 0xff00ffffffUL;
}
