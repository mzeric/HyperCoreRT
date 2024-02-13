#include <vmmio.h>
#include "processor.h"
#include "cpu_inline_asm.h"

void do_bad_mode(struct cpu_user_regs *regs, int magic) {

    vmm_debug("trap->%p, %d\n", regs->lr, magic);
    while(1);
}

void do_hyper_sync(struct cpu_user_regs *regs, int magic) {
	uint64_t esr = mrs(esr_el2);
	uint64_t far = mrs(far_el2);
	uint64_t elr = mrs(elr_el2);
    vmm_info("hyper sync excep: %p\n", regs);
    vmm_info("trap->%p,  sp=%x, %d\n", regs->lr, regs->sp, magic);

    vmm_info("esr: %x, far:%x, elr:%x\n", esr, far, elr);

    uint32_t ec = (esr &0xFC000000LU)>>26;
    vmm_info("excep reason:%x\n", ec);
}