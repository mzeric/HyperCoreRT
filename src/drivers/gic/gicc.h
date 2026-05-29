#pragma once


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



#define GICC_CTL_ENABLE 0x1
#define GICC_CTL_EOI    (0x1 << 9)

#define GICC_IA_IRQ       0x03ff
#define GICC_IA_CPU_MASK  0x1c00
#define GICC_IA_CPU_SHIFT 10