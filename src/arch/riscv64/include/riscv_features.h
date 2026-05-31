#pragma once
#include "htypes.h"

void riscv_features_init(void *fdt);
bool riscv_has_svvptc(void);
bool riscv_has_fpu(void);
bool riscv_has_vector(void);
u32 riscv_timebase_frequency(void);
