#pragma once
#include "config.h"
#include "system.h"
#include "vmmio.h"
#include "htypes.h"
#include "gic.h"
#include "page.h"
/*
 * GIC System register assembly aliases picked from kernel
 */
#define ICC_PMR_EL1               S3_0_C4_C6_0
#define ICC_DIR_EL1               S3_0_C12_C11_1
#define ICC_SGI1R_EL1             S3_0_C12_C11_5
#define ICC_EOIR1_EL1             S3_0_C12_C12_1
#define ICC_IAR1_EL1              S3_0_C12_C12_0
#define ICC_BPR1_EL1              S3_0_C12_C12_3
#define ICC_CTLR_EL1              S3_0_C12_C12_4
#define ICC_SRE_EL1               S3_0_C12_C12_5
#define ICC_IGRPEN1_EL1           S3_0_C12_C12_7
#define ICC_IGRPEN0_EL1           S3_0_C12_C12_6


#define ICH_VSEIR_EL2             S3_4_C12_C9_4
#define ICC_SRE_EL2               S3_4_C12_C9_5
#define ICH_HCR_EL2               S3_4_C12_C11_0
#define ICH_VTR_EL2               S3_4_C12_C11_1
#define ICH_MISR_EL2              S3_4_C12_C11_2
#define ICH_EISR_EL2              S3_4_C12_C11_3
#define ICH_ELSR_EL2              S3_4_C12_C11_5
#define ICH_VMCR_EL2              S3_4_C12_C11_7
#define ZCR_EL2                   S3_4_C1_C2_0

#define __LR0_EL2(x)              S3_4_C12_C12_ ## x
#define __LR8_EL2(x)              S3_4_C12_C13_ ## x

#define ICH_LR0_EL2               __LR0_EL2(0)
#define ICH_LR1_EL2               __LR0_EL2(1)
#define ICH_LR2_EL2               __LR0_EL2(2)
#define ICH_LR3_EL2               __LR0_EL2(3)
#define ICH_LR4_EL2               __LR0_EL2(4)
#define ICH_LR5_EL2               __LR0_EL2(5)
#define ICH_LR6_EL2               __LR0_EL2(6)
#define ICH_LR7_EL2               __LR0_EL2(7)
#define ICH_LR8_EL2               __LR8_EL2(0)
#define ICH_LR9_EL2               __LR8_EL2(1)
#define ICH_LR10_EL2              __LR8_EL2(2)
#define ICH_LR11_EL2              __LR8_EL2(3)
#define ICH_LR12_EL2              __LR8_EL2(4)
#define ICH_LR13_EL2              __LR8_EL2(5)
#define ICH_LR14_EL2              __LR8_EL2(6)
#define ICH_LR15_EL2              __LR8_EL2(7)

#define __AP0Rx_EL2(x)            S3_4_C12_C8_ ## x
#define ICH_AP0R0_EL2             __AP0Rx_EL2(0)
#define ICH_AP0R1_EL2             __AP0Rx_EL2(1)
#define ICH_AP0R2_EL2             __AP0Rx_EL2(2)
#define ICH_AP0R3_EL2             __AP0Rx_EL2(3)

#define __AP1Rx_EL2(x)            S3_4_C12_C9_ ## x
#define ICH_AP1R0_EL2             __AP1Rx_EL2(0)
#define ICH_AP1R1_EL2             __AP1Rx_EL2(1)
#define ICH_AP1R2_EL2             __AP1Rx_EL2(2)
#define ICH_AP1R3_EL2             __AP1Rx_EL2(3)


/*
 * Additional registers defined in GIC v3.
 * Common GICD registers are defined in gic.h
 */

#define GICD_STATUSR                 (0x010)
#define GICD_SETSPI_NSR              (0x040)
#define GICD_CLRSPI_NSR              (0x048)
#define GICD_SETSPI_SR               (0x050)
#define GICD_CLRSPI_SR               (0x058)
#define GICD_IGRPMODR                (0xD00)
#define GICD_IGRPMODRN               (0xD7C)
#define GICD_IROUTER                 (0x6000)
#define GICD_IROUTER32               (0x6100)
#define GICD_IROUTER1019             (0x7FD8)
#define GICD_PIDR2                   (0xFFE8)

/* Common between GICD_PIDR2 and GICR_PIDR2 */
#define GIC_PIDR2_ARCH_MASK         (0xf0)
#define GIC_PIDR2_ARCH_GICv3        (0x30)
#define GIC_PIDR2_ARCH_GICv4        (0x40)

#define GICC_SRE_EL2_SRE             (1UL << 0)
#define GICC_SRE_EL2_DFB             (1UL << 1)
#define GICC_SRE_EL2_DIB             (1UL << 2)
#define GICC_SRE_EL2_ENEL1           (1UL << 3)

#define GICC_IAR_INTID_MASK          (0xFFFFFF)

/* Additional bits in GICD_TYPER defined by GICv3 */
#define GICD_TYPE_ID_BITS_SHIFT 19
#define GICD_TYPE_ID_BITS(r)    ((((r) >> GICD_TYPE_ID_BITS_SHIFT) & 0x1f) + 1)

#define GICD_TYPE_LPIS               (1U << 17)

#define GICD_CTLR_RWP                (1UL << 31)
#define GICD_CTLR_ARE_NS             (1U << 4)
#define GICD_CTLR_ENABLE_G1A         (1U << 1)
#define GICD_CTLR_ENABLE_G1          (1U << 0)
#define GICD_IROUTER_SPI_MODE_ANY    (1UL << 31)

#define GICC_CTLR_EL1_EOImode_drop   (1U << 1)

#define GICR_WAKER_ProcessorSleep    (1U << 1)
#define GICR_WAKER_ChildrenAsleep    (1U << 2)

#define GICR_SYNCR_NOT_BUSY          1
/*
 * Implementation defined value JEP106?
 * use physical hw value for now
 */
#define GICV3_GICD_IIDR_VAL          0x34c
#define GICV3_GICR_IIDR_VAL          GICV3_GICD_IIDR_VAL

/* Two pages for the RD_base and SGI_base register frame. */
#define GICV3_GICR_SIZE              (2 * SZ_64K)

#define GICR_CTLR                    (0x0000)
#define GICR_IIDR                    (0x0004)
#define GICR_TYPER                   (0x0008)
#define GICR_STATUSR                 (0x0010)
#define GICR_WAKER                   (0x0014)
#define GICR_SETLPIR                 (0x0040)
#define GICR_CLRLPIR                 (0x0048)
#define GICR_PROPBASER               (0x0070)
#define GICR_PENDBASER               (0x0078)
#define GICR_INVLPIR                 (0x00A0)
#define GICR_INVALLR                 (0x00B0)
#define GICR_SYNCR                   (0x00C0)
#define GICR_PIDR2                   GICD_PIDR2

/* GICR for SGI's & PPI's */

#define GICR_IGROUPR0                (0x0080)
#define GICR_ISENABLER0              (0x0100)
#define GICR_ICENABLER0              (0x0180)
#define GICR_ISPENDR0                (0x0200)
#define GICR_ICPENDR0                (0x0280)
#define GICR_ISACTIVER0              (0x0300)
#define GICR_ICACTIVER0              (0x0380)
#define GICR_IPRIORITYR0             (0x0400)
#define GICR_IPRIORITYR7             (0x041C)
#define GICR_ICFGR0                  (0x0C00)
#define GICR_ICFGR1                  (0x0C04)
#define GICR_IGRPMODR0               (0x0D00)
#define GICR_NSACR                   (0x0E00)

#define GICR_CTLR_ENABLE_LPIS        (1U << 0)

#define GICR_TYPER_PLPIS             (1U << 0)
#define GICR_TYPER_VLPIS             (1U << 1)
#define GICR_TYPER_LAST              (1U << 4)
#define GICR_TYPER_PROC_NUM_SHIFT    8
#define GICR_TYPER_PROC_NUM_MASK     (0xffff << GICR_TYPER_PROC_NUM_SHIFT)

/* For specifying the inner cacheability type only */
#define GIC_BASER_CACHE_nCnB         0ULL
/* For specifying the outer cacheability type only */
#define GIC_BASER_CACHE_SameAsInner  0ULL
#define GIC_BASER_CACHE_nC           1ULL
#define GIC_BASER_CACHE_RaWt         2ULL
#define GIC_BASER_CACHE_RaWb         3ULL
#define GIC_BASER_CACHE_WaWt         4ULL
#define GIC_BASER_CACHE_WaWb         5ULL
#define GIC_BASER_CACHE_RaWaWt       6ULL
#define GIC_BASER_CACHE_RaWaWb       7ULL
#define GIC_BASER_CACHE_MASK         7ULL

#define GIC_BASER_NonShareable       0ULL
#define GIC_BASER_InnerShareable     1ULL
#define GIC_BASER_OuterShareable     2ULL

#define GICR_PROPBASER_OUTER_CACHEABILITY_SHIFT         56
#define GICR_PROPBASER_OUTER_CACHEABILITY_MASK               \
        (7ULL << GICR_PROPBASER_OUTER_CACHEABILITY_SHIFT)
#define GICR_PROPBASER_SHAREABILITY_SHIFT               10
#define GICR_PROPBASER_SHAREABILITY_MASK                     \
        (3ULL << GICR_PROPBASER_SHAREABILITY_SHIFT)
#define GICR_PROPBASER_INNER_CACHEABILITY_SHIFT         7
#define GICR_PROPBASER_INNER_CACHEABILITY_MASK               \
        (7ULL << GICR_PROPBASER_INNER_CACHEABILITY_SHIFT)
#define GICR_PROPBASER_RES0_MASK                             \
        (GENMASK_ULL(63, 59) | GENMASK_ULL(55, 52) | GENMASK_ULL(6, 5))

#define GICR_PENDBASER_SHAREABILITY_SHIFT               10
#define GICR_PENDBASER_INNER_CACHEABILITY_SHIFT         7
#define GICR_PENDBASER_OUTER_CACHEABILITY_SHIFT         56
#define GICR_PENDBASER_SHAREABILITY_MASK                     \
	(3UL << GICR_PENDBASER_SHAREABILITY_SHIFT)
#define GICR_PENDBASER_INNER_CACHEABILITY_MASK               \
	(7UL << GICR_PENDBASER_INNER_CACHEABILITY_SHIFT)
#define GICR_PENDBASER_OUTER_CACHEABILITY_MASK               \
        (7ULL << GICR_PENDBASER_OUTER_CACHEABILITY_SHIFT)
#define GICR_PENDBASER_PTZ                              BIT(62, ULL)
#define GICR_PENDBASER_RES0_MASK                             \
        (BIT(63, ULL) | GENMASK_ULL(61, 59) | GENMASK_ULL(55, 52) |  \
         GENMASK_ULL(15, 12) | GENMASK_ULL(6, 0))

#define DEFAULT_PMR_VALUE            0xff

#define LPI_PROP_PRIO_MASK           0xfc
#define LPI_PROP_RES1                (1 << 1)
#define LPI_PROP_ENABLED             (1 << 0)

#define ICH_VMCR_EOI                 (1 << 9)
#define ICH_VMCR_VENG1               (1 << 1)
#define ICH_VMCR_PRIORITY_MASK       0xff
#define ICH_VMCR_PRIORITY_SHIFT      24

#define ICH_LR_VIRTUAL_MASK          0xffff
#define ICH_LR_VIRTUAL_SHIFT         0
#define ICH_LR_CPUID_MASK            0x7
#define ICH_LR_CPUID_SHIFT           10
#define ICH_LR_PHYSICAL_MASK         0x3ff
#define ICH_LR_PHYSICAL_SHIFT        32
#define ICH_LR_STATE_MASK            0x3
#define ICH_LR_STATE_SHIFT           62
#define ICH_LR_STATE_PENDING         (1ULL << 62)
#define ICH_LR_STATE_ACTIVE          (1ULL << 63)
#define ICH_LR_PRIORITY_MASK         0xff
#define ICH_LR_PRIORITY_SHIFT        48
#define ICH_LR_HW_MASK               0x1
#define ICH_LR_HW_SHIFT              61
#define ICH_LR_GRP_MASK              0x1
#define ICH_LR_GRP_SHIFT             60
#define ICH_LR_MAINTENANCE_IRQ       (1ULL << 41)
#define ICH_LR_GRP1                  (1ULL << 60)
#define ICH_LR_HW                    (1ULL << 61)

#define ICH_VTR_NRLRGS               0x3f
#define ICH_VTR_PRIBITS_MASK         0x7
#define ICH_VTR_PRIBITS_SHIFT        29

#define ICH_SGI_IRQMODE_SHIFT        40
#define ICH_SGI_IRQMODE_MASK         0x1
#define ICH_SGI_TARGET_OTHERS        1ULL
#define ICH_SGI_TARGET_LIST          0
#define ICH_SGI_IRQ_SHIFT            24
#define ICH_SGI_IRQ_MASK             0xf
#define ICH_SGI_TARGETLIST_MASK      0xffff
#define ICH_SGI_AFFx_MASK            0xff
#define ICH_SGI_AFFINITY_LEVEL(x)    (16 * (x))

struct rdist_region {
    paddr_t base;
    paddr_t size;
    void  *map_base;
    int single_rdist;
};

#define isb() asm ("isb":::"memory")
/* Access to system registers */
#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)

#define WRITE_SYSREG64(v, name) do {                    \
    uint64_t _r = v;                                    \
    asm volatile("msr "__stringify(name)", %0" : : "r" (_r));       \
} while (0)
#define READ_SYSREG64(name) ({                          \
    uint64_t _r;                                        \
    asm volatile("mrs  %0, "__stringify(name) : "=r" (_r));         \
    _r; })

#define READ_SYSREG(name)     READ_SYSREG64(name)
#define WRITE_SYSREG(v, name) WRITE_SYSREG64(v, name)

/* Wrappers for accessing interrupt controller list registers. */
#define ICH_LR_REG(index)          ICH_LR ## index ## _EL2
#define WRITE_SYSREG_LR(v, index)  WRITE_SYSREG(v, ICH_LR_REG(index))
#define READ_SYSREG_LR(index)      READ_SYSREG(ICH_LR_REG(index))

/* 64K per frame
    0. RD
    1. SIG PPI
*/
#define GICD_RDIST_SGI_BASE 0x10000

void init_gicv2(void *gicd_base, void* gicc_base);
void init_gicv3(void *gicd_base, void* gicr_base);
