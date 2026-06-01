#pragma once

#include "vcpu.h"

class RiscvVcpuContextManager final {
public:
    static void Save(vcpu_t *vcpu);
    static void Restore(vcpu_t *vcpu);

private:
    RiscvVcpuContextManager() = delete;
};
