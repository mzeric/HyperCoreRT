#pragma once

#define NR_GIC_LOCAL_IRQS  NR_LOCAL_IRQS
#define NR_GIC_SGI         16

#include "gic_common.h"
#include "gic_common_private.h"

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

/* for GICv2 */
#define GICC_CTLR       (0x0000)
#define GICC_PMR        (0x0004)
#define GICC_BPR        (0x0008)
#define GICC_IAR        (0x000C)
#define GICC_EOIR       (0x0010)
#define GICC_RPR        (0x0014)
#define GICC_HPPIR      (0x0018)
#define GICC_APR        (0x00D0)
#define GICC_NSAPR      (0x00E0)
#define GICC_IIDR       (0x00FC)
#define GICC_DIR        (0x1000)

#define GICH_HCR        (0x00)
#define GICH_VTR        (0x04)
#define GICH_VMCR       (0x08)
#define GICH_MISR       (0x10)
#define GICH_EISR0      (0x20)
#define GICH_EISR1      (0x24)
#define GICH_ELSR0      (0x30)
#define GICH_ELSR1      (0x34)
#define GICH_APR        (0xF0)
#define GICH_LR         (0x100)

/* Register bits */
#define GICD_CTL_ENABLE 0x1

#define GICD_TYPE_LINES 0x01f
#define GICD_TYPE_CPUS_SHIFT 5
#define GICD_TYPE_CPUS  0x0e0
#define GICD_TYPE_SEC   0x400
#define GICD_TYPER_DVIS (1U << 18)

#define GICC_CTL_ENABLE 0x1
#define GICC_CTL_EOI    (0x1 << 9)

#define GICC_IA_IRQ       0x03ff
#define GICC_IA_CPU_MASK  0x1c00
#define GICC_IA_CPU_SHIFT 10

#define GICH_HCR_EN       (1 << 0)
#define GICH_HCR_UIE      (1 << 1)
#define GICH_HCR_LRENPIE  (1 << 2)
#define GICH_HCR_NPIE     (1 << 3)
#define GICH_HCR_VGRP0EIE (1 << 4)
#define GICH_HCR_VGRP0DIE (1 << 5)
#define GICH_HCR_VGRP1EIE (1 << 6)
#define GICH_HCR_VGRP1DIE (1 << 7)

#define GICH_MISR_EOI     (1 << 0)
#define GICH_MISR_U       (1 << 1)
#define GICH_MISR_LRENP   (1 << 2)
#define GICH_MISR_NP      (1 << 3)
#define GICH_MISR_VGRP0E  (1 << 4)
#define GICH_MISR_VGRP0D  (1 << 5)
#define GICH_MISR_VGRP1E  (1 << 6)
#define GICH_MISR_VGRP1D  (1 << 7)