#pragma once

#include "arch_regs.h"
#include "excep.h"
#include "htypes.h"

enum class Aarch64GuestFaultError : unsigned int {
    no_current_task,
    no_current_vcpu,
    unmapped_ipa,
    non_mmio_access_fault,
    emulate_failed,
    sysreg_emulate_failed,
    unsupported_instruction_abort,
    unsupported_data_abort,
    unsupported_exception_class,
};

enum class Aarch64GuestFaultAction : unsigned int {
    resume_guest,
    yield_scheduler,
};

class Aarch64GuestFaultResult final {
public:
    static constexpr Aarch64GuestFaultResult resume_guest() noexcept {
        return Aarch64GuestFaultResult(true, Aarch64GuestFaultAction::resume_guest,
                                       Aarch64GuestFaultError::unsupported_exception_class);
    }

    static constexpr Aarch64GuestFaultResult yield_scheduler() noexcept {
        return Aarch64GuestFaultResult(true, Aarch64GuestFaultAction::yield_scheduler,
                                       Aarch64GuestFaultError::unsupported_exception_class);
    }

    static constexpr Aarch64GuestFaultResult failure(Aarch64GuestFaultError error) noexcept {
        return Aarch64GuestFaultResult(false, Aarch64GuestFaultAction::resume_guest, error);
    }

    constexpr bool ok() const noexcept { return is_ok; }
    constexpr bool is_err() const noexcept { return !is_ok; }
    constexpr Aarch64GuestFaultAction action() const noexcept { return action_value; }
    constexpr Aarch64GuestFaultError error() const noexcept { return error_value; }

private:
    constexpr Aarch64GuestFaultResult(bool ok_value,
                                      Aarch64GuestFaultAction action,
                                      Aarch64GuestFaultError error) noexcept
        : action_value(action), error_value(error), is_ok(ok_value) {}

    Aarch64GuestFaultAction action_value;
    Aarch64GuestFaultError error_value;
    bool is_ok;
};

class Aarch64GuestFaultManager final {
public:
    static Aarch64GuestFaultResult handle_exception(struct cpu_user_regs *regs,
                                                    const union esr &esr);

private:
    Aarch64GuestFaultManager() = delete;

    static Aarch64GuestFaultResult handle_sysreg(struct cpu_user_regs *regs,
                                                 const union esr &esr);
    static Aarch64GuestFaultResult handle_instruction_abort(struct cpu_user_regs *regs,
                                                            const union esr &esr);
    static Aarch64GuestFaultResult handle_data_abort(struct cpu_user_regs *regs,
                                                     const union esr &esr);
    static Aarch64GuestFaultResult handle_hvc(struct cpu_user_regs *regs);
};
