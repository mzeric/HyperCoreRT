#include "inline_asm.h"
#include "safe_printf.h"
#include "htypes.h"
#include "include/arch_regs.h"
#include "timer.h"
#include "sched.h"
#include "mm.h"
#include "mmu.h"
#include "exception.h"

#define irq_printf safe_printf

struct fault_info {
    int (*fn)(struct cpu_user_regs *regs, const char *name);
    const char *name;
};

void panic(const char *msg) {
    safe_printf("Kernel panic: %s\n", msg ? msg : "");
    while (1)
        ;
}

void show_regs(struct cpu_user_regs *regs) {
    safe_printf("sepc: %016lx ra : %016lx sp : %016lx\n", regs->sepc, regs->ra, regs->sp);
    safe_printf(" gp : %016lx tp : %016lx t0 : %016lx\n", regs->gp, regs->tp, regs->t0);
    safe_printf(" t1 : %016lx t2 : %016lx t3 : %016lx\n", regs->t1, regs->t2, regs->s0);
    safe_printf(" s1 : %016lx a0 : %016lx a1 : %016lx\n", regs->s1, regs->a0, regs->a1);
    safe_printf(" a2 : %016lx a3 : %016lx a4 : %016lx\n", regs->a2, regs->a3, regs->a4);
    safe_printf(" a5 : %016lx a6 : %016lx a7 : %016lx\n", regs->a5, regs->a6, regs->a7);
    safe_printf(" s2 : %016lx s3 : %016lx s4 : %016lx\n", regs->s2, regs->s3, regs->s4);
    safe_printf(" s5 : %016lx s6 : %016lx s7 : %016lx\n", regs->s5, regs->s6, regs->s7);
    safe_printf(" s8 : %016lx s9 : %016lx s10: %016lx\n", regs->s8, regs->s9, regs->s10);
    safe_printf(" s11: %016lx t3 : %016lx t4: %016lx\n", regs->s11, regs->t3, regs->t4);
    safe_printf(" t5 : %016lx t6 : %016lx\n", regs->t5, regs->t6);
}

static void do_trap_error(struct cpu_user_regs *regs, const char *str) {
    safe_printf("Oops - %s\n", str);
    show_regs(regs);

    u64 hs = csrr(CSR_HSTATUS);
    u64 stval = csrr(CSR_STVAL);

    safe_printf("sstatus:0x%016lx  hs:%lx, stval: %lx, sbadaddr:0x%016lx  scause:0x%016lx\n",
                regs->sstatus, hs, stval,
                regs->sbadaddr,
                regs->scause);

    panic("trap error");
}

#define DO_ERROR_INFO(name)                                                                        \
    int name(struct cpu_user_regs *regs, const char *str) {                                              \
        do_trap_error(regs, str);                                                                  \
        return 0;                                                                                  \
    }

DO_ERROR_INFO(do_trap_unknown);
DO_ERROR_INFO(do_trap_insn_misaligned);
DO_ERROR_INFO(do_trap_insn_fault);
DO_ERROR_INFO(do_trap_insn_illegal);
DO_ERROR_INFO(do_trap_load_misaligned);
DO_ERROR_INFO(do_trap_load_fault);
DO_ERROR_INFO(do_trap_store_misaligned);
DO_ERROR_INFO(do_trap_store_fault);
DO_ERROR_INFO(do_trap_ecall_u);
DO_ERROR_INFO(do_trap_ecall_s);
DO_ERROR_INFO(do_trap_break);
// DO_ERROR_INFO(do_page_fault);
int inject_illegal_inst(struct cpu_user_regs *regs, uint64_t inst);

int is_trap_from_guest(u64 hstatus) { return (hstatus & HSTATUS_SPV); }


int do_page_fault(struct  cpu_user_regs *regs, const char *str) {
    do_trap_error(regs, str);
    return 0;
}

int do_illegal_inst(struct cpu_user_regs *regs, int inst) {

    // inject_illegal_inst(regs, inst);

    do_trap_error(regs, "");

    return 0;
}

extern void *_trap_stack_top_;
static int system_opcode_insn(struct cpu_user_regs *regs, u64 inst) {
    int   rc = 0, do_write, rs1_num;

    u64 rs1_val, csr_num, csr_val, new_csr_val;

    safe_printf("opcode:%ld\n", __LINE__);
    if ((inst & INSN_MASK_WFI) == INSN_MATCH_WFI) {
        /* Wait for irq with default timeout */
        irq_printf("get wfi: %lx\n", &_trap_stack_top_);
        // do_illegal_inst(regs, inst);
        #if 0
        vcpu_irq_wait_timeout(vcpu, 0);
        #endif
        goto done;
    }
    safe_printf("opcode:%ld\n", __LINE__);

    rs1_num = (inst >> 15) & 0x1f;
    rs1_val = GET_RS1(inst, regs);
    csr_num = inst >> 20;
#if 0
    rc = cpu_vcpu_csr_read(vcpu, csr_num, &csr_val);
#endif
    if (rc < 0) {
        return do_illegal_inst(regs, inst);
    }    safe_printf("opcode:%ld\n", __LINE__);

    if (rc) {
        return rc;
    }
    safe_printf("opcode:%ld\n", __LINE__);

    do_write = rs1_num;
    switch (GET_RM(inst)) {
    case 1:
        new_csr_val = rs1_val;
        do_write = 1;
        break;
    case 2:
        new_csr_val = csr_val | rs1_val;
        break;
    case 3:
        new_csr_val = csr_val & ~rs1_val;
        break;
    case 5:
        new_csr_val = rs1_num;
        do_write = 1;
        break;
    case 6:
        new_csr_val = csr_val | rs1_num;
        break;
    case 7:
        new_csr_val = csr_val & ~rs1_num;
        break;
    default:
        return do_illegal_inst(regs, inst);
    };
    safe_printf("opcode:%ld\n", __LINE__);

    if (do_write) {
#if 0
        rc = cpu_vcpu_csr_write(vcpu, csr_num, new_csr_val);
#endif
        if (rc  <0) {
            return do_illegal_inst(regs, inst);
        }
        if (rc) {
            return rc;
        }
    }

    SET_RD(inst, regs, csr_val);

done:
	regs->sepc += 4;
    safe_printf("opcode:%ld\n", __LINE__);

	return rc;
}

void handle_virt_instruction(struct cpu_user_regs *regs) {
    u64 stval = csrr(CSR_STVAL);
    safe_printf("virt inst: %lx, %lx\n", csrr(CSR_HSTATUS), csrr(CSR_STVAL));

    if (stval & 0x3 != 0x3) {

        if (stval == 0) {
            do_trap_error(regs, "inst trap");
        }

        if (stval & 0x3 != 3)
            do_illegal_inst(regs, stval);
    }

    switch ((stval & INSN_OPCODE_MASK) >> INSN_OPCODE_SHIFT) {
    case INSN_OPCODE_SYSTEM:
        system_opcode_insn(regs, stval);
        return;
    default:
        do_illegal_inst(regs, stval);
        return;
    };
    do_trap_error(regs, "");
}

int do_guest_page_fault(struct cpu_user_regs *regs, const char *str) {
    int cause = regs->scause & 0x1F;
    switch(cause) {
        case RISCV_EXCP_VIRT_INSTRUCTION_FAULT:
        handle_virt_instruction(regs);
        break;
    }

    return 0;
}

static const struct fault_info fault_info[] = {
    {do_trap_insn_misaligned, "Instruction address misaligned"},
    {do_trap_insn_fault, "Instruction access fault"},
    {do_trap_insn_illegal, "Illegal instruction"},
    {do_trap_break, "Breakpoint"},
    {do_trap_load_misaligned, "Load address misaligned"},
    {do_trap_load_fault, "Load access fault"},
    {do_trap_store_misaligned, "Store/AMO address misaligned"},
    {do_trap_store_fault, "Store/AMO access fault"},
    {do_trap_ecall_u, "Environment call from U/VU-mode"},
    {do_trap_ecall_s, "Environment call from S/HS-mode"},
    {do_trap_unknown, "Environment call from VS-mode"}, // 0xA TODO: sbi-trap
    {do_trap_unknown, "Environment call from M-mode"},
    {do_page_fault, "Instruction page fault"},
    {do_page_fault, "Load page fault"},
    {do_trap_unknown, "unknown"},//0xE
    {do_page_fault, "Store/AMO page fault"},// 0xF
    {do_trap_unknown, "semi-host"}, // 0x10
    {do_trap_unknown, "unknown"}, // 0x11
    {do_trap_unknown, "unknown"}, // 0x12
    {do_trap_unknown, "unknown"}, // 0x13
    {do_guest_page_fault, "guest instruction page fault"}, // 0x14
    {do_guest_page_fault, "guest load access fault"},
    {do_guest_page_fault, "guest virt instruction fault"},
    {do_guest_page_fault, "guest store AMO accesss fault"},// 0x17
};

struct interrupt_info {
    int irq;
    const char *brief;
};
static const struct interrupt_info interrupt_info[] = {
    {0, "reserved"},
    {1, "S mode's software irq"},
    {2, "VS mode's virtual software irq"},
    {3, "M mode's software irq"},
    {4, "unknown"},
    {5, "S mode's timer irq"},
    {6, "VS mode's virtual timer irq"},
};

static inline const struct fault_info *ec_to_fault_info(unsigned int scause) {
    return fault_info + (scause & 0x1F);
}

void hfence(void);
void do_stage2_fault(struct cpu_user_regs *regs, int cause) {
    u64 htval = csrr(CSR_HTVAL);//htval
    u64 stval = csrr(stval);

    u64 fault_addr = (htval << 2) | (stval & 3);

    safe_printf("stage 2 fault addr:%lx\n", fault_addr);

    fault_addr &= ~(PAGE_SIZE - 1);
    safe_printf("stage2 fault addr:%lx\n", fault_addr);
    if (fault_addr == 0x30000000)
        pg_map_stage2(
            0x30000000, 0x10000000, 0x1000, PAGE_ATTR_READ | PAGE_ATTR_WRITE | PAGE_ATTR_USER, 0);
    hfence();

}

extern "C" void do_exception(struct cpu_user_regs *args, u64 cause) {
    const struct fault_info *inf;

    if (cause & (1ul << 63)) {
        cause &= ~(0x1u<<63);
        u64 cur_sp;
        asm ("mv %0, sp":"=memory"(cur_sp)::);
        safe_printf("get interrupt: 0x%x, sp: %lx, cur_sp:%lx\n", cause, args->sp, cur_sp);
        switch (cause)
        {
        case IRQ_S_TIMER:
            handle_timer_irq();
            sched_yield(args);

            break;
        case IRQ_VS_SOFT:
        case 9:
            /* external irq */
        case 1:
            /* IPI */
        default:
            safe_printf("unknown irq:%d, %d\n", cause, is_trap_from_guest(csrr(hstatus)));
            break;
        }
    } else {

        safe_printf("got excep: 0x%x, hs:%d\n", cause, is_trap_from_guest(csrr(hstatus)));
        switch (cause) {
        case 8:
            break;
        case RISCV_EXCP_VS_ECALL:
            safe_printf("virtual ecall %d\n", args->a7);
            do_trap_error(args, "");
            break;
        case 0x13:
            safe_printf("get 13\n");
        case 0x14:
            safe_printf("stage2 page fetch fault\n");
        case 0x15:
            safe_printf("stage2 page load fault\n");
        case 0x17:
            safe_printf("stage2 page save fault\n");
            do_stage2_fault(args, cause);
            do_trap_error(args, "");
            break;
        default:
            inf = ec_to_fault_info(cause);

            if (!inf->fn(args, inf->name))
            do_trap_error(args, "");
                // return;
        }
    }
}

void setup_exception(void*entry) {
    // csrw(CSR_SSCRATCH, 0);
    csrw(stvec, entry);

    csrw(sie, -1);
   	csrs(sstatus, 0x2u);

}