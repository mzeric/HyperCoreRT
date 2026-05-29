#pragma once
#include "compiler.h"

#define GICD_SGI_TARGET_LIST_SHIFT   (24)
#define GICD_SGI_TARGET_LIST_MASK    (0x3UL << GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_LIST         (0UL<<GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_LIST_VAL     (0)
#define GICD_SGI_TARGET_OTHERS       (1UL<<GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_OTHERS_VAL   (1)
#define GICD_SGI_TARGET_SELF         (2UL<<GICD_SGI_TARGET_LIST_SHIFT)
#define GICD_SGI_TARGET_SELF_VAL     (2)
#define GICD_SGI_TARGET_SHIFT        (16)
#define GICD_SGI_TARGET_MASK         (0xFFUL<<GICD_SGI_TARGET_SHIFT)
#define GICD_SGI_GROUP1              (1UL<<15)
#define GICD_SGI_INTID_MASK          (0xFUL)


/*******************************************************************************
 * GIC Distributor interface general definitions
 ******************************************************************************/
/* Constants to categorise interrupts */
#define MIN_SGI_ID		U(0)
#define MIN_SEC_SGI_ID		U(8)
#define MIN_PPI_ID		U(16)
#define MIN_SPI_ID		U(32)
#define MAX_SPI_ID		U(1019)

#define TOTAL_SPI_INTR_NUM	(MAX_SPI_ID - MIN_SPI_ID + U(1))
#define TOTAL_PCPU_INTR_NUM	(MIN_SPI_ID - MIN_SGI_ID)

/* Mask for the priority field common to all GIC interfaces */
#define GIC_PRI_MASK			U(0xff)

/* Mask for the configuration field common to all GIC interfaces */
#define GIC_CFG_MASK			U(0x3)

/* Constant to indicate a spurious interrupt in all GIC versions */
#define GIC_SPURIOUS_INTERRUPT		U(1023)

/* Interrupt configurations: 2-bit fields with LSB reserved */
#define GIC_INTR_CFG_LEVEL		(0 << 1)
#define GIC_INTR_CFG_EDGE		(1 << 1)

/* Highest possible interrupt priorities */
#define GIC_HIGHEST_SEC_PRIORITY	U(0x00)
#define GIC_HIGHEST_NS_PRIORITY		U(0x80)

/*******************************************************************************
 * Common GIC Distributor interface register offsets
 ******************************************************************************/
#define GICD_CTLR		U(0x0)
#define GICD_TYPER		U(0x4)
#define GICD_IIDR		U(0x8)
#define GICD_IGROUPR		U(0x80)
#define GICD_ISENABLER		U(0x100)
#define GICD_ICENABLER		U(0x180)
#define GICD_ISPENDR		U(0x200)
#define GICD_ICPENDR		U(0x280)
#define GICD_ISACTIVER		U(0x300)
#define GICD_ICACTIVER		U(0x380)
#define GICD_IPRIORITYR		U(0x400)
#define GICD_ICFGR		U(0xc00)
#define GICD_NSACR		U(0xe00)

/* GICD_CTLR bit definitions */
#define CTLR_ENABLE_G0_SHIFT		0
#define CTLR_ENABLE_G0_MASK		U(0x1)
#define CTLR_ENABLE_G0_BIT		BIT_32(CTLR_ENABLE_G0_SHIFT)

/*******************************************************************************
 * Common GIC Distributor interface register constants
 ******************************************************************************/
#define PIDR2_ARCH_REV_SHIFT	4
#define PIDR2_ARCH_REV_MASK	U(0xf)

/* GIC revision as reported by PIDR2.ArchRev register field */
#define ARCH_REV_GICV1		U(0x1)
#define ARCH_REV_GICV2		U(0x2)
#define ARCH_REV_GICV3		U(0x3)
#define ARCH_REV_GICV4		U(0x4)

#define IGROUPR_SHIFT		5
#define ISENABLER_SHIFT		5
#define ICENABLER_SHIFT		ISENABLER_SHIFT
#define ISPENDR_SHIFT		5
#define ICPENDR_SHIFT		ISPENDR_SHIFT
#define ISACTIVER_SHIFT		5
#define ICACTIVER_SHIFT		ISACTIVER_SHIFT
#define IPRIORITYR_SHIFT	2
#define ITARGETSR_SHIFT		2
#define ICFGR_SHIFT		4
#define NSACR_SHIFT		4

/* GICD_TYPER shifts and masks */
#define TYPER_IT_LINES_NO_SHIFT	U(0)
#define TYPER_IT_LINES_NO_MASK	U(0x1f)

/* Value used to initialize Normal world interrupt priorities four at a time */
#define GICD_IPRIORITYR_DEF_VAL			\
	(GIC_HIGHEST_NS_PRIORITY	|	\
	(GIC_HIGHEST_NS_PRIORITY << 8)	|	\
	(GIC_HIGHEST_NS_PRIORITY << 16)	|	\
	(GIC_HIGHEST_NS_PRIORITY << 24))


#define GICD_CTLR_RWP                (1UL << 31)
#define GICD_CTLR_ARE_NS             (1U << 4)
#define GICD_CTLR_ENABLE_G1A         (1U << 1)
#define GICD_CTLR_ENABLE_G1          (1U << 0)
#define GICD_IROUTER_SPI_MODE_ANY    (1UL << 31)
