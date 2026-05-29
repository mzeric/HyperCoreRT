#pragma once

#define SATP_MODE_OFF			(0ul)
#define SATP_MODE_SV32			(1ul)
#define SATP_MODE_SV39			(8ul)
#define SATP_MODE_SV48			(9ul)
#define SATP_MODE_SV57			(10ul)
#define SATP_MODE_SV64			(11ul)


#include "riscv_csr.h"
#ifndef  __ASSEMBLY__
#include "inst_decode.h"
#include "riscv_status.h"

#endif


#define RISCV_SCRATCH_SMP_ID_OFFSET     (0 * __SIZEOF_POINTER__)
#define RISCV_SCRATCH_EXCE_STACK_OFFSET (1 * __SIZEOF_POINTER__)
#define RISCV_SCRATCH_TMP0_OFFSET       (2 * __SIZEOF_POINTER__)
#define RISCV_SCRATCH_TMP1_OFFSET       (3 * __SIZEOF_POINTER__)
#define RISCV_SCRATCH_SIZE              64
