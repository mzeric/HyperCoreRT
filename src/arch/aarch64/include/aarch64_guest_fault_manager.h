#pragma once

#include "arch_regs.h"
#include "excep.h"
#include "htypes.h"

enum aarch64_guest_fault_error {
    AARCH64_GUEST_FAULT_NO_CURRENT_TASK,
    AARCH64_GUEST_FAULT_NO_CURRENT_VCPU,
    AARCH64_GUEST_FAULT_UNMAPPED_IPA,
    AARCH64_GUEST_FAULT_NON_MMIO_ACCESS_FAULT,
    AARCH64_GUEST_FAULT_EMULATE_FAILED,
    AARCH64_GUEST_FAULT_SYSREG_EMULATE_FAILED,
    AARCH64_GUEST_FAULT_UNSUPPORTED_INSTRUCTION_ABORT,
    AARCH64_GUEST_FAULT_UNSUPPORTED_DATA_ABORT,
    AARCH64_GUEST_FAULT_UNSUPPORTED_EXCEPTION_CLASS,
};

enum aarch64_guest_fault_action {
    AARCH64_GUEST_FAULT_RESUME_GUEST,
    AARCH64_GUEST_FAULT_YIELD_SCHEDULER,
};

struct aarch64_guest_fault_result {
    enum aarch64_guest_fault_action action;
    enum aarch64_guest_fault_error error;
    int ok;
};

static inline struct aarch64_guest_fault_result aarch64_guest_fault_resume_guest(void) {
    struct aarch64_guest_fault_result result = {
        .action = AARCH64_GUEST_FAULT_RESUME_GUEST,
        .error = AARCH64_GUEST_FAULT_UNSUPPORTED_EXCEPTION_CLASS,
        .ok = 1,
    };
    return result;
}

static inline struct aarch64_guest_fault_result aarch64_guest_fault_yield_scheduler(void) {
    struct aarch64_guest_fault_result result = {
        .action = AARCH64_GUEST_FAULT_YIELD_SCHEDULER,
        .error = AARCH64_GUEST_FAULT_UNSUPPORTED_EXCEPTION_CLASS,
        .ok = 1,
    };
    return result;
}

static inline struct aarch64_guest_fault_result
aarch64_guest_fault_failure(enum aarch64_guest_fault_error error) {
    struct aarch64_guest_fault_result result = {
        .action = AARCH64_GUEST_FAULT_RESUME_GUEST,
        .error = error,
        .ok = 0,
    };
    return result;
}

struct aarch64_guest_fault_result
aarch64_guest_fault_handle_exception(struct cpu_user_regs *regs, const union esr *esr);
