#include "vmio.h"
#include "processor.h"
#include "inline_asm.h"
#include "excep.h"
#include "page.h"
#include "aarch64_system.h"
#include "src/drivers/gic/gicv3.h"
#include "list.h"
#include "mm.h"
#include "sched.h"
#include "timer.h"
#include "emulate.h"
#include "traps_helper.h"

#include "emul_gic.h"
#include "emul_psci.h"
#include "emul_timer.h"
#include "hyper_config.h"
#include "src/drivers/pl011/pl011.h"
#include "emul_uart.h"
#include "sys_reg.h"
#include "smp.h"
#include "ipi.h"

#include <stdio.h> /* just remove guest warning */
#include <string.h>
#include <ioremap.h>
#include <guest_memory.h>

static void pl011_maybe_receive(void) {
    uart_emul_service_host();
}

static int is_uart_ipa(paddr_t ipa) {
    struct hyper_config *cfg = hyper_config();

    return cfg->uart.enabled && cfg->uart.guest_base <= ipa &&
           ipa < cfg->uart.guest_base + cfg->uart.guest_size;
}

/*

exception entry (from ARM:D1.10 P1799):

On taking an exception to AArch64 state:
1. The PE state is saved in the SPSR_ELx at the Exception level the exception is taken to. See Saved Program Status Registers (SPSRs) on page D1-1786.
2. The preferred return address is saved in the ELR_ELx at the Exception level the exception is taken to. See Exception Link Registers (ELRs) on page D1-1790.
3. All of PSTATE.{D, A, I, F} are set to 1. See Process state, PSTATE on page D1-1791.
4. PSTATE.UAO is set to 0. See Process state, PSTATE on page D1-1791.
5. If the exception is a synchronous exception or an SError interrupt, information characterizing the reason for the exception is saved in the ESR_ELx at the Exception level the exception is taken to. See Use of the ESR_EL1, ESR_EL2, and ESR_EL3 on page D1-1801.
6. Execution moves to the target Exception level, and starts at the address defined by the exception vector. Which exception vector is used is also an indicator of whether the exception came from a lower Exception level or the current Exception level. See Exception vectors on page D1-1800.
7. The stack pointer register selected is the dedicated stack pointer register for the target Exception level. See The stack pointer registers on page D1-1785.


Exception return:

In AArch64 state, an ERET instruction causes an exception return, see ERET on page C6-622. On executing an ERET instruction at ELx:
• The PC is restored with the value held in ELR_ELx.
• PSTATE is restored by using the contents of SPSR_ELx.

*/




void destroy_task(hyper_task_t *task) {
    if (task->regs.sp)
        kfree((void *)task->regs.sp);
}

void do_bad_mode(struct cpu_user_regs *regs, int is_compat) {

    hyper_debug("sysr: 0x%lx %d", regs->cpsr, is_compat);
    hyper_debug("el:%lx", mrs(CurrentEL));


}

void dump_regs(struct cpu_user_regs *regs) {
    hyper_printf("x0=%lx x1=%lx x2=%lx x3=%lx\n",
                 regs->x0, regs->x1, regs->x2, regs->x3);
    hyper_printf("x4=%lx x5=%lx x6=%lx x7=%lx\n",
                 regs->x4, regs->x5, regs->x6, regs->x7);
    hyper_printf("x8=%lx x9=%lx x10=%lx x11=%lx\n",
                 regs->x8, regs->x9, regs->x10, regs->x11);
    hyper_printf("x12=%lx x13=%lx x14=%lx x15=%lx\n",
                 regs->x12, regs->x13, regs->x14, regs->x15);
    hyper_printf("x16=%lx x17=%lx x18=%lx x19=%lx\n",
                 regs->x16, regs->x17, regs->x18, regs->x19);
    hyper_printf("x20=%lx x21=%lx x22=%lx x23=%lx\n",
                 regs->x20, regs->x21, regs->x22, regs->x23);
    hyper_printf("x24=%lx x25=%lx x26=%lx x27=%lx\n",
                 regs->x24, regs->x25, regs->x26, regs->x27);
    hyper_printf("x28=%lx fp=%lx lr=%lx\n",
                 regs->x28, regs->fp, regs->lr);
    hyper_printf("sp=%lx pc=%lx cpsr=%lx\n",
                 regs->sp, regs->pc, regs->cpsr);
}

static void dump_vcpu_state(const char *tag) {
    hyper_task_t *t = current_task();
    if (!t) return;
    hyper_printf("[%s][vCPU%d] ELR_EL2=%lx SPSR_EL2=%lx FAR_EL2=%lx\n",
                 tag, t->id, mrs(elr_el2), mrs(spsr_el2), mrs(far_el2));
    hyper_printf("[%s][vCPU%d] ESR_EL2=%lx\n",
                 tag, t->id, mrs(esr_el2));
    hyper_printf("[%s][vCPU%d] SCTLR_EL1=%lx TTBR0=%lx TTBR1=%lx TCR=%lx\n",
                 tag, t->id, mrs(sctlr_el1), mrs(ttbr0_el1), mrs(ttbr1_el1), mrs(tcr_el1));
    hyper_printf("[%s][vCPU%d] SP_EL0=%lx SP_EL1=%lx ELR_EL1=%lx\n",
                 tag, t->id, mrs(sp_el0), mrs(sp_el1), mrs(elr_el1));
    hyper_printf("[%s][vCPU%d] traps=%lu irqs=%lu switches=%lu pending_virq=%d\n",
                 tag, t->id, t->trap_count, t->irq_count, t->switch_count,
                 t->pending_virq_count);
    /* Dump LR state */
    for (int i = 0; i < 4; i++) {
        u64 lr;
        switch(i) {
        case 0: lr = mrs_s(sys_reg(3,4,12,12,0)); break;
        case 1: lr = mrs_s(sys_reg(3,4,12,12,1)); break;
        case 2: lr = mrs_s(sys_reg(3,4,12,12,2)); break;
        case 3: lr = mrs_s(sys_reg(3,4,12,12,3)); break;
        }
        if (lr)
            hyper_printf("[%s][vCPU%d] LR%d=%lx\n", tag, t->id, i, lr);
    }
}

void irq_delay(int v) {
    u64 start = get_cycles();
    while(get_cycles() < start + v);
}

void do_irq_mode(struct cpu_user_regs *regs, int is_compat) {
    int hirq_no;
    hirq_no = mrs(ICC_IAR1_EL1);
    if (current_task())
        current_task()->irq_count++;

    /* SGI (0-15): host IPI */
    if (hirq_no < 16) {
        ipi_handle((uint8_t)hirq_no);
        gicv3_eof_int(hirq_no);
        sched_yield(regs);
        return;
    }

    if(hirq_no == hyper_config()->timer.hyp_timer_ppi) {
        gicv3_reenable_hyp_timer_ppi();
        hyp_timer_rearm();
        sched_yield(regs);
        pl011_maybe_receive();
        gicv3_eof_int(hirq_no);
    } else if(hirq_no == hyper_config()->timer.guest_virt_timer_ppi) {
        /* Virtual timer fired against the currently running guest vCPU. */
        u64 ctl = mrs(cntv_ctl_el0);
        msr(cntv_ctl_el0, ctl | (1u << 1));  /* IT_MASK = 1 */
        hyper_task_t *cur = current_task();
        if (cur)
            gic_vcpu_inject_virq(cur, hyper_config()->timer.guest_virt_timer_ppi);
        gicv3_eof_int(hirq_no);
        sched_yield(regs);  /* may switch vCPU; flushes LR via __el2_switch_to */
        /* In case no switch happened we still need to flush LR for the cur task */
        if (current_task() == cur && cur)
            gic_vcpu_flush_lr(cur);
    } else if(hyper_config()->uart.enabled && hirq_no == hyper_config()->uart.host_irq) {
        pl011_maybe_receive();
        gicv3_eof_int(hirq_no);
    } else {
        gicv3_eof_int(hirq_no);
        hyper_warn("[pCPU%d] unsupported irq: %d current=%p", cpu_id(), hirq_no, current_task());

        /* Don't hang — just return and let the CPU continue */
        return;
    }
}
/* irq interrupt EL1 */
void do_guest_irq(struct cpu_user_regs *regs) {
    do_irq_mode(regs, 0);
    return;
}

uint64_t get_gva() { return mrs(FAR_EL2); }

paddr_t get_ipa() {
    vaddr_t gva = mrs(FAR_EL2);
    paddr_t hp = mrs(HPFAR_EL2);

    paddr_t ipa = (paddr_t)(hp & HPFAR_MASK) << (12 - 4);
    ipa |= gva & ~PAGE_MASK;

    return ipa;
}

static int s2_map(vcpu_t *vcpu, const uint64_t ipa);

int do_guest_inst_abort_trap(struct cpu_user_regs *regs, const union esr esr) {

    vcpu_t *vcpu = current_task()->vcpu;
    paddr_t ipa = get_ipa();
    int fsc = esr.iabt.fsc;

    if (fsc >= FSC_FLT_TRANS && fsc <= FSC_FLT_TRANS + 3) {
        return s2_map(vcpu, ipa);
    }

    print_iss_detail(esr);
    return 0;
}


int s2_map(vcpu_t *vcpu, const uint64_t ipa) {

    struct mem_region *mem = guest_mem_find_region(vcpu, ipa, 0);
    if (!mem) {
        safe_printf("invalid ipa: %lx\n", ipa);
        return -1;
    }
    size_t base = mem->gpa;

    paddr_t ipa_aligned = ipa & ~(PAGE_SIZE - 1);

    size_t  offset = (ipa_aligned - base);
    paddr_t hpa_aligned = mem->hpa + offset;
    stage2_map(&vcpu->mm_info, ipa_aligned, hpa_aligned, PAGE_SIZE, MEM_NORMAL_RW, mem->attr);

    /* Invalidate stage-2 TLB for the IPA we just mapped so the guest's
       retry will pick up the new PTE. */
    asm volatile("tlbi IPAS2E1, %0" :: "r"(ipa_aligned));
    asm volatile("dsb ish");
    asm volatile("isb");

    return 0;
}

int do_stage2_data_abort_trap(struct cpu_user_regs *regs, const union esr esr) {

    paddr_t ipa = get_ipa();

    int fsc = esr.dabt.fsc;
    int d_reg = esr.dabt.reg;
    int d_size = esr.dabt.size;
    int ret = 0;


    /* here stage2 mmap or emulate */

    vcpu_t *vcpu = current_task()->vcpu;

    switch (fsc) {
    case FSC_FLT_TRANS ... FSC_FLT_TRANS + 3: {

       ret = s2_map(vcpu, ipa);
    } break;

    case FSC_FLT_ACCESS ... FSC_FLT_ACCESS + 3: {
        if (esr.dabt.write) {
            vcpu_emulate_write(vcpu, regs, ipa, d_reg, d_size);
        } else {
            vcpu_emulate_read(vcpu, regs, ipa, d_reg, d_size);

        }

        regs->pc += 4;
    } break;
    }

    if(ret) {
        panic("do_data_abort failed\n");
    }

    return 0;
}

int do_guest_msr_mrs_trap(struct cpu_user_regs *regs, const union esr esr) {
    uint64_t iss = esr.iss;
    int reg_id = esr.sysreg.reg;
    int size = esr.sysreg.len;
    uint64_t data;
    int ret = 0;
    if (esr.sysreg.read) {
        safe_printf("mrs trap: %d, size:%d\n", reg_id, size);
        ret = vcpu_emulate_sysreg_read(regs, iss, &data);
        vcpu_reg_write(regs, reg_id, size, data);


    } else {
        data = vcpu_reg_read(regs, reg_id, size);
        ret = vcpu_emulate_sysreg_write(regs, iss, data);
    }

    regs->pc += 4;
		// while(1);

    return ret;
}

void do_guest_exception(struct cpu_user_regs *regs, int is_compat) {
    int ret = 0;
    if (is_compat == 1) {
        panic("Not support AArch32 Mode\n");
    }
    // uint64_t elr = mrs(elr_el1); /* elr_el1 != elr_el2 */
    // uint64_t elr2 = mrs(elr_el2);
    // uint64_t spsr_el1 = mrs(spsr_el1);

    // safe_printf("GUEST excep spsr:%x, elr_el1:%lx, elr_el2:%lx\n", regs->cpsr, elr, mrs(elr_el2));
    // safe_printf("esr_el1: %x, esr_el2:%x\n", mrs(esr_el1), mrs(esr_el2));
    const union esr esr = { .bits = mrs(esr_el2) };

    if (current_task())
        current_task()->trap_count++;

    switch (esr.ec) {
    case HSR_EC_SYSREG:
        ret = do_guest_msr_mrs_trap(regs, esr);
        if (!ret) {
            gicv3_reenable_hyp_timer_ppi();
            hyp_timer_rearm();
            return;
        }
        break;
    case HSR_EC_INSTR_ABORT_LOWER_EL:
        ret = do_guest_inst_abort_trap(regs, esr);
        if (!ret) return;   /* resolved – skip sched_yield */
        break;
    case HSR_EC_DATA_ABORT_LOWER_EL:
        ret = do_stage2_data_abort_trap(regs, esr);
        if (!ret) return;   /* resolved – skip sched_yield, return to guest */
        break;
    case HSR_EC_HVC64:
        psci_vcpu_call(regs);
        // sched_yield(regs);
        break;
    default: {
        dump_vcpu_state("EXCEPTION");
        hyper_info("[vCPU%d] unknown exception EC:0x%x", current_task()->id, esr.ec);
        panic("");
    }
    }
    if(ret) {
        dump_vcpu_state("EXCEPTION-FAIL");
        panic("guest exception failed\n");
    }
    sched_yield(regs);
}

void do_hyper_sync(struct cpu_user_regs *regs, int magic) {
    uint64_t esr = mrs(esr_el2);
    uint64_t far = mrs(far_el2);
    uint64_t elr = mrs(elr_el2);
    int ec = esr >> 26;

    dump_vcpu_state("HYPER-SYNC");
    dump_regs(regs);

    safe_printf("hyper-sync, spsr: 0x%x, esr: %x, far:%lx,elr:%x, lr:%x\n", regs->cpsr, esr,
            far, elr, regs->lr);

    safe_printf("stlr_el2:%lx\n", mrs(sctlr_el2));
    safe_printf("Exception details: EC:0x%x, ISS:0x%x\n", ec, esr & 0x1ffffff);

    safe_printf("spsr:%x, hcr_el2:%x\n", regs->cpsr, mrs(hcr_el2));
    const union esr esru = {.bits = esr};

    print_iss_detail(esru);
    while(1);
    panic("hyper_sync");
}

void on_guest_excep_return(void ){

}

