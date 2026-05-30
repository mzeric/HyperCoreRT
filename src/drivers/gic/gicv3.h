#pragma once
#include "config.h"
#include "aarch64_system.h"
#include "vmio.h"
#include "htypes.h"
#include "gic.h"
#include "sys_reg.h"

#define GICR_SGI_FRAME_OFFSET 0x10000UL
#define GICR_SGI_BASE_FIXMAP  0xE100010000UL

#define GIC_SPI_INTID(spi)    (32U + (spi))

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
#define ICC_SRE_EL2_S             S3_4_C12_C9_5
#define ICC_SRE_EL2               sys_reg(3, 4, 12, 9, 5)
#define ICH_HCR_EL2               S3_4_C12_C11_0
#define ICH_VTR_EL2               S3_4_C12_C11_1
#define ICH_MISR_EL2              S3_4_C12_C11_2
#define ICH_EISR_EL2              S3_4_C12_C11_3
#define ICH_ELSR_EL2              S3_4_C12_C11_5
#define ICH_VMCR_EL2              S3_4_C12_C11_7
#define ZCR_EL2                   S3_4_C1_C2_0

#define ICC_SRE_EL3		S3_6_C12_C12_5

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

#if 0
struct rdist_region {
    paddr_t base;
    paddr_t size;
    void  *map_base;
    int single_rdist;
};
#endif

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

void init_gicv2(void *gicd_base, void *gicc_base);
void init_gicv3(void *gicd_base, void *gicc_base, void *gicr_base);
void gicv3_pcpu_init(int cpu_id);
void gicv3_eof_int(int id);
void gicv3_reenable_hyp_timer_ppi(void);
void wakeup_gic(uintptr_t gicr_base);
int  gicv3_init_el3(uintptr_t gicr_base);