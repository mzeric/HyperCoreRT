#include "riscv_guest_fault_manager.h"

#include "emulate.h"
#include "exception.h"
#include "guest_memory.h"
#include "inline_asm.h"
#include "inst_decode.h"
#include "mm.h"
#include "mmu.h"
#include "riscv_features.h"
#include "riscv_sbi_manager.h"
#include "riscv_status.h"
#include "safe_printf.h"
#include "sched.h"

void hfence(void);

bool riscv_guest_fault_is_guest_trap(u64 hstatus) {
    return (hstatus & HSTATUS_SPV) != 0;
}

static vcpu_t *current_vcpu(void) {
    hyper_task_t *task = current_task();
    return task ? task->vcpu : NULL;
}

static u64 stage2_fault_addr(u64 htval, u64 stval) {
    return (htval << 2) | (stval & 3);
}

static u64 guest_page_fault_cause(u64 cause) {
    switch (cause) {
    case RISCV_EXCP_INST_GUEST_PAGE_FAULT:
        return RISCV_EXCP_INST_PAGE_FAULT;
    case RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT:
        return RISCV_EXCP_STORE_PAGE_FAULT;
    case RISCV_EXCP_LOAD_GUEST_ACCESS_FAULT:
    default:
        return RISCV_EXCP_LOAD_PAGE_FAULT;
    }
}

static int redirect_guest_exception(struct cpu_user_regs *regs, u64 scause, u64 stval) {
    struct cpu_vcpu_trap trap = {0};
    trap.sepc = regs->sepc;
    trap.scause = scause;
    trap.stval = stval;

    if (vcpu_redirect_trap(regs, &trap) != 0)
        return -1;
    return 0;
}

static int handle_stage2_fault(struct cpu_user_regs *regs, u64 cause) {
    u64 htval = csrr(CSR_HTVAL);
    u64 stval = csrr(CSR_STVAL);
    u64 fault_addr = stage2_fault_addr(htval, stval);
    u64 fault_page = fault_addr & ~(PAGE_SIZE - 1);
    int is_write = (cause == RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT);

    vcpu_t *vcpu = current_vcpu();
    if (!vcpu) {
        safe_printf("guest stage2 fault without vcpu: addr=%lx cause=%lu\n",
                    fault_addr, cause);
        return -1;
    }

    struct mem_region *region = guest_mem_find_region(vcpu, fault_addr, 0);
    if (!region) {
        safe_printf("guest stage2 fault: unmapped addr=%lx cause=%lu\n",
                    fault_addr, cause);
        return redirect_guest_exception(regs, guest_page_fault_cause(cause),
                                      stval ? stval : fault_addr);
    }

    if (region->dev) {
        if (vcpu_emulate_mmio(vcpu, regs, fault_addr, is_write) != 0) {
            safe_printf("guest stage2 fault: mmio emulate failed addr=%lx cause=%lu\n",
                        fault_addr, cause);
            return redirect_guest_exception(regs, guest_page_fault_cause(cause),
                                          stval ? stval : fault_addr);
        }
        return 0;
    }

    u64 offset = fault_page - region->gpa;
    u64 hpa = region->hpa + offset;
    pg_map_stage2(fault_page, hpa, PAGE_SIZE, region->attr, 0);
    if (!riscv_has_svvptc())
        hfence();

    return 0;
}

static int handle_virtual_instruction(struct cpu_user_regs *regs) {
    u64 inst = csrr(CSR_STVAL);

    if ((inst & INSN_MASK_WFI) == INSN_MATCH_WFI) {
        regs->sepc += 4;
        return 0;
    }

    safe_printf("guest virtual instruction fault: sepc=%lx inst=%lx\n", regs->sepc, inst);
    return redirect_guest_exception(regs, RISCV_EXCP_ILLEGAL_INST, inst);
}

int riscv_guest_fault_handle_exception(struct cpu_user_regs *regs, u64 cause) {
    switch (cause) {
    case RISCV_EXCP_VS_ECALL:
        riscv_sbi_handle_vs_ecall(regs);
        return 0;
    case RISCV_EXCP_INST_GUEST_PAGE_FAULT:
    case RISCV_EXCP_LOAD_GUEST_ACCESS_FAULT:
    case RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT:
        return handle_stage2_fault(regs, cause);
    case RISCV_EXCP_VIRT_INSTRUCTION_FAULT:
        return handle_virtual_instruction(regs);
    default:
        if (riscv_guest_fault_is_guest_trap(csrr(CSR_HSTATUS)))
            return redirect_guest_exception(regs, cause, csrr(CSR_STVAL));
        return -1;
    }
}
