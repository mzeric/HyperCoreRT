#pragma once

#include "htypes.h"

enum riscv_fence_kind {
    RISCV_FENCE_REMOTE_FENCE_I,
    RISCV_FENCE_REMOTE_HFENCE_VVMA,
    RISCV_FENCE_REMOTE_HFENCE_VVMA_ASID,
    RISCV_FENCE_REMOTE_HFENCE_GVMA,
    RISCV_FENCE_REMOTE_HFENCE_GVMA_VMID,
};

long riscv_remote_fence(enum riscv_fence_kind kind, u64 hart_mask, u64 hart_base);
