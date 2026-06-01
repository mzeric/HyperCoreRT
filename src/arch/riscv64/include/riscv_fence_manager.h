#pragma once

#include "htypes.h"

enum class RiscvFenceKind : u32 {
    RemoteFenceI,
    RemoteHfenceVvma,
    RemoteHfenceVvmaAsid,
    RemoteHfenceGvma,
    RemoteHfenceGvmaVmid,
};

class RiscvFenceManager final {
public:
    static long RemoteFence(RiscvFenceKind kind, u64 hart_mask, u64 hart_base);

private:
    RiscvFenceManager() = delete;
};
