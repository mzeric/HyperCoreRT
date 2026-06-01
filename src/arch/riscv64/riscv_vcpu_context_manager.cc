#include "riscv_vcpu_context_manager.h"

void RiscvVcpuContextManager::Save(vcpu_t *vcpu) {
    vcpu_context_save(vcpu);
}

void RiscvVcpuContextManager::Restore(vcpu_t *vcpu) {
    vcpu_context_restore(vcpu);
}
