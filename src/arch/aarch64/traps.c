#include <vmmio.h>
#include "processor.h"
#include "cpu_inline_asm.h"
#include "execp.h"
#include "page.h"

/*

Execption entry (from ARM:D1.10 P1799):

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
void do_bad_mode(struct cpu_user_regs *regs, int is_compat) {

    vmm_debug("sysr: 0x%p\n", regs->cpsr);
    vmm_debug("el:%d\n", mrs(CurrentEL));
    while(1);
}

uint64_t get_gva() { mrs(FAR_EL2); }

paddr_t get_ipa() {
    vaddr_t gva = mrs(FAR_EL2);
    paddr_t hp = mrs(HPFAR_EL2);

    paddr_t ipa = (paddr_t)(hp & HPFAR_MASK) << (12 - 4);
    ipa |= gva & ~PAGE_MASK;

    return ipa;
}


void do_guest_exception(struct cpu_user_regs *regs, int is_compat) {

    if(is_compat) {
        panic("Not support AArch32 Mode\n");
    }
    uint64_t elr = mrs(elr_el1); /* elr_el1 != elr_el2 */
    uint64_t elr2 = mrs(elr_el2);
    uint64_t spsr_el1 = mrs(spsr_el1);

    vmm_debug("GUEST excep spsr:%x, elr:%x\n", regs->cpsr, elr);
    vmm_debug("el:%d\n", mrs(CurrentEL));

    const union esr esr = { .bits = regs->esr };

    vmm_info("Exception details: EC:0x%x, ISS:0x%x\n", esr.ec, esr.iss);

    switch(esr.ec) {
        case HSR_EC_INSTR_ABORT_LOWER_EL:
            vmm_info("guest iabt addr:%p\n", elr);
            vmm_info("guest need stage2 mmap here\n");
            vmm_info("gva:%p, ipa:%p\n", get_gva(), get_ipa());
            break;
        case HSR_EC_DATA_ABORT_LOWER_EL:
            vmm_info("guest dabt addr:%p\n", elr);
            vmm_info("guest need stage2 mmap here\n");
            break;
        default:
            vmm_info("unknown exception\n");

    }
    while(1);
}

void guest_entry(void) {
    while(1);
}

uint64_t get_default_hcr_flags(void)
{
    return  (HCR_PTW|HCR_BSU_INNER|HCR_AMO|HCR_IMO|HCR_FMO|HCR_VM|
             HCR_TID3|HCR_TSC|HCR_TAC|HCR_SWIO|HCR_TIDCP|HCR_FB|HCR_TSW);
}


void switch_to_el1(void) {
    // msr(sctlr_el1, 0);


    // hcr_val &= ~1;//disable vmmu;


    msr(elr_el1, guest_entry);
    msr(sp_el1, 0);
    msr(spsr_el2, 0x3c5);
    asm volatile("eret\t\n":::"memory");
    while(1);
}

void do_hyper_sync(struct cpu_user_regs *regs, int magic) {
	uint64_t esr = mrs(esr_el2);
	uint64_t far = mrs(far_el2);
	uint64_t elr = mrs(elr_el2);
    vmm_info("spsr: 0x%x, esr: %x, far:%x, elr:%x\n", regs->cpsr, esr, far, regs->lr);

    int ec = esr >> 26;
    vmm_info("Exception details: EC:0x%x, ISS:0x%x\n", ec, esr & 0x1ffffff);

    // switch_to_el1();

    vmm_info("spsr:%x, hcr_el2:%x\n", regs->cpsr, mrs(hcr_el2));
    panic("panic");
}

void on_guest_excep_return(void ){

}