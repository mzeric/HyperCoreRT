#include "aarch64_guest_fault_manager.h"

#include "aarch64_hcr.h"
#include "arch_page.h"
#include "emul_psci.h"
#include "emulate.h"
#include "guest_memory.h"
#include "inline_asm.h"
#include "mmu.h"
#include "page.h"
#include "safe_printf.h"
#include "sched.h"
#include "timer.h"
#include "traps_helper.h"
#include "src/drivers/gic/gicv3.h"

static vcpu_t *current_vcpu(void) {
    hyper_task_t *task = current_task();
    return task ? task->vcpu : NULL;
}

static paddr_t fault_ipa(void) {
    vaddr_t gva = mrs(FAR_EL2);
    paddr_t hp = mrs(HPFAR_EL2);

    paddr_t ipa = (paddr_t)(hp & HPFAR_MASK) << (12 - 4);
    ipa |= gva & ~PAGE_MASK;
    return ipa;
}

static bool is_translation_fault(int fsc) {
    return fsc >= FSC_FLT_TRANS && fsc <= FSC_FLT_TRANS + 3;
}

static bool is_access_fault(int fsc) {
    return fsc >= FSC_FLT_ACCESS && fsc <= FSC_FLT_ACCESS + 3;
}

static Aarch64GuestFaultResult map_stage2_page(vcpu_t *vcpu, paddr_t ipa) {
    if (!vcpu)
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::no_current_vcpu);

    struct mem_region *mem = guest_mem_find_region(vcpu, ipa, 0);
    if (!mem) {
        safe_printf("guest stage2 fault: unmapped ipa=%lx\n", ipa);
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::unmapped_ipa);
    }

    paddr_t ipa_aligned = ipa & ~(PAGE_SIZE - 1);
    paddr_t hpa_aligned = mem->hpa + (ipa_aligned - mem->gpa);
    stage2_map(&vcpu->mm_info, ipa_aligned, hpa_aligned, PAGE_SIZE, MEM_NORMAL_RW,
               mem->attr);
    tlb_inv_guest_ipa(ipa_aligned);

    return Aarch64GuestFaultResult::resume_guest();
}

static Aarch64GuestFaultResult emulate_mmio_access(vcpu_t *vcpu,
                                                   struct cpu_user_regs *regs,
                                                   paddr_t ipa,
                                                   const union esr &esr) {
    if (!vcpu)
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::no_current_vcpu);

    struct mem_region *mem = guest_mem_find_region(vcpu, ipa, 0);
    if (!mem) {
        safe_printf("guest data abort: unmapped mmio ipa=%lx\n", ipa);
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::unmapped_ipa);
    }

    if (!mem->dev || !mem->dev->driver || !mem->dev->driver->ops)
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::non_mmio_access_fault);

    if (esr.dabt.write && !mem->dev->driver->ops->write)
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::emulate_failed);
    if (!esr.dabt.write && !mem->dev->driver->ops->read)
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::emulate_failed);

    int ret = esr.dabt.write ?
        vcpu_emulate_write(vcpu, regs, ipa, esr.dabt.reg, esr.dabt.size) :
        vcpu_emulate_read(vcpu, regs, ipa, esr.dabt.reg, esr.dabt.size);
    if (ret != 0)
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::emulate_failed);

    regs->pc += 4;
    return Aarch64GuestFaultResult::resume_guest();
}

Aarch64GuestFaultResult Aarch64GuestFaultManager::handle_sysreg(
    struct cpu_user_regs *regs, const union esr &esr) {
    if (!current_task())
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::no_current_task);
    if (!current_vcpu())
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::no_current_vcpu);

    uint64_t iss = esr.iss;
    int reg_id = esr.sysreg.reg;
    int size = esr.sysreg.len;
    uint64_t data = 0;
    int ret;

    if (esr.sysreg.read) {
        safe_printf("mrs trap: %d, size:%d\n", reg_id, size);
        ret = vcpu_emulate_sysreg_read(regs, iss, &data);
        if (ret == 0)
            vcpu_reg_write(regs, reg_id, size, data);
    } else {
        data = vcpu_reg_read(regs, reg_id, size);
        ret = vcpu_emulate_sysreg_write(regs, iss, data);
    }

    if (ret != 0)
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::sysreg_emulate_failed);

    regs->pc += 4;
    gicv3_reenable_hyp_timer_ppi();
    hyp_timer_rearm();
    return Aarch64GuestFaultResult::resume_guest();
}

Aarch64GuestFaultResult Aarch64GuestFaultManager::handle_instruction_abort(
    struct cpu_user_regs *regs, const union esr &esr) {
    (void)regs;

    int fsc = esr.iabt.fsc;
    if (is_translation_fault(fsc))
        return map_stage2_page(current_vcpu(), fault_ipa());

    print_iss_detail(esr);
    return Aarch64GuestFaultResult::failure(
        Aarch64GuestFaultError::unsupported_instruction_abort);
}

Aarch64GuestFaultResult Aarch64GuestFaultManager::handle_data_abort(
    struct cpu_user_regs *regs, const union esr &esr) {
    paddr_t ipa = fault_ipa();
    int fsc = esr.dabt.fsc;
    vcpu_t *vcpu = current_vcpu();

    if (is_translation_fault(fsc))
        return map_stage2_page(vcpu, ipa);

    if (is_access_fault(fsc))
        return emulate_mmio_access(vcpu, regs, ipa, esr);

    print_iss_detail(esr);
    return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::unsupported_data_abort);
}

Aarch64GuestFaultResult Aarch64GuestFaultManager::handle_hvc(
    struct cpu_user_regs *regs) {
    if (!current_task())
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::no_current_task);
    if (!current_vcpu())
        return Aarch64GuestFaultResult::failure(Aarch64GuestFaultError::no_current_vcpu);

    (void)psci_vcpu_call(regs);
    return Aarch64GuestFaultResult::yield_scheduler();
}

Aarch64GuestFaultResult Aarch64GuestFaultManager::handle_exception(
    struct cpu_user_regs *regs, const union esr &esr) {
    switch (esr.ec) {
    case HSR_EC_SYSREG:
        return handle_sysreg(regs, esr);
    case HSR_EC_INSTR_ABORT_LOWER_EL:
        return handle_instruction_abort(regs, esr);
    case HSR_EC_DATA_ABORT_LOWER_EL:
        return handle_data_abort(regs, esr);
    case HSR_EC_HVC64:
        return handle_hvc(regs);
    default:
        return Aarch64GuestFaultResult::failure(
            Aarch64GuestFaultError::unsupported_exception_class);
    }
}
