#include "htypes.h"
#include "arch_regs.h"
#include "vmio.h"
#include "inline_asm.h"

void do_guest_irq(struct cpu_user_regs *regs) {
    safe_printf("guest <><><>\n");
}

void do_guest_exception(struct cpu_user_regs *regs) {
    safe_printf("guest <><><>\n");

}

void do_hyper_sync(struct cpu_user_regs *regs, int magic) {
    uint64_t esr = mrs(esr_el1);
    uint64_t far = mrs(far_el1);
    uint64_t elr = mrs(elr_el1);
    int ec = esr >> 26;

    safe_printf("hyper-sync, spsr: 0x%x, esr: %x, far:%lx,elr:%x, lr:%x\n", regs->cpsr, esr,
            far, elr, regs->lr);

    safe_printf("stlr_el2:%lx\n", mrs(sctlr_el1));
    safe_printf("Exception details: EC:0x%x, ISS:0x%x\n", ec, esr & 0x1ffffff);
    while(1);
}

void do_bad_mode(struct cpu_user_regs *regs,  int is_compat) {
    safe_printf("guest <><><>\n");

}
void do_irq_mode(struct cpu_user_regs *regs, int is_compat) {
    safe_printf("guest <><><>\n");

}