#pragma once

/*
 * Shareability values for LPAE table/block entries.  Only the values
 * actually emitted by the entry builders are listed here; the bit
 * pattern matches the architectural encoding.
 */
#define LPAE_SH_NON_SHAREABLE 0x0
#define LPAE_SH_OUTER         0x2
#define LPAE_SH_INNER         0x3

/*
 * Attribute Index values for the stage-1 AttrIndx[2:0] field.
 *
 * Each value is an index into the byte array that lives in MAIR_EL1
 * (or MAIR_EL2 for hypervisor mappings).  The MAIR contents are set
 * up by the boot code and define what every index actually means.
 */
#define MT_DEVICE_nGnRnE 0x0
#define MT_NORMAL_NC     0x1
#define MT_NORMAL_WT     0x2
#define MT_NORMAL_WB     0x3
#define MT_DEVICE_nGnRE  0x4
#define MT_NORMAL        0x7

typedef uint64_t mfn_t;
