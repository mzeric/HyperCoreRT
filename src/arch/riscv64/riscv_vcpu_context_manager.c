#include "riscv_vcpu_context_manager.h"

void riscv_vcpu_context_save_manager(vcpu_t *vcpu) {
    vcpu_context_save(vcpu);
}

void riscv_vcpu_context_restore_manager(vcpu_t *vcpu) {
    vcpu_context_restore(vcpu);
}
