#pragma once

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