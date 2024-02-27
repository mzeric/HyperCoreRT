#include "vmmio.h"
#include "processor.h"
#include "cpu_inline_asm.h"
#include "execp.h"
#include "page.h"
#include "system.h"
#include "gicv3.h"

#include<stdio.h> /* just remove guest warning */

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

    vmm_debug("sysr: 0x%p %d\n", regs->cpsr, is_compat);
    vmm_debug("el:%d\n", mrs(CurrentEL));


    while(1);
}

void do_irq_mode(struct cpu_user_regs *regs, int is_compat) {

    vmm_debug("sysr: 0x%p %d\n", regs->cpsr, is_compat);
    vmm_debug("el:%d\n", current_el());
    int id = mrs(ICC_IAR1_EL1);
    vmm_info("irq-%d\n", id);

}

uint64_t get_gva() { return mrs(FAR_EL2); }

paddr_t get_ipa() {
    vaddr_t gva = mrs(FAR_EL2);
    paddr_t hp = mrs(HPFAR_EL2);

    paddr_t ipa = (paddr_t)(hp & HPFAR_MASK) << (12 - 4);
    ipa |= gva & ~PAGE_MASK;

    return ipa;
}

int do_stage2_data_abort_trap(struct cpu_user_regs *regs, const union esr esr){
    print_iss_detail(esr);
}

void print_iss_detail(const union esr esr) {
    int fsc = esr.dabt.fsc;

    switch (fsc) {
    case 0 ... 3:
        vmm_info("FSC: address size fault level:%d\n", fsc & FSC_LL_MASK);
        break;
    case FSC_FLT_TRANS ... FSC_FLT_TRANS + 3:
        vmm_info("FSC: translation fault level:%d\n", fsc & FSC_LL_MASK);
        break;
    case FSC_FLT_ACCESS ... FSC_FLT_ACCESS + 3:
        vmm_info("FSC: access flag fault level:%d\n", fsc & FSC_LL_MASK);
        break;
    case FSC_FLT_PERM ... FSC_FLT_PERM + 3:
        vmm_info("FSC: permission fault level:%d\n", fsc & FSC_LL_MASK);
        break;
    case FSC_SEA:
        vmm_info("FSC: Synchronous external abort\n");
        break;
    case FSC_SPE:
        vmm_info("FSC: Synchronous parity or ECC error on memory access, "
                 "not on translation table walk\n");
        break;
    case FSC_APE:
        vmm_info("FSC:FSC_APE\n");
        break;
    case FSC_SEATT ... FSC_SEATT + 3:
        vmm_info("FSC: Synchronous external abort, on translation table "
                 "walk, level:%d\n",
                fsc & FSC_LL_MASK);
        break;
    case FSC_SPETT ... FSC_SPETT + 3:
        vmm_info("FSC: Synchronous parity or ECC error on memory access on "
                 "translation table walk, level:%d\n",
                fsc & FSC_LL_MASK);
        break;

    case FSC_AF:
        vmm_info("FSC: Alignment fault\n");
        break;
    case FSC_TLB_FLT:
        vmm_info("FSC: TLB conflict abort\n");
        break;
    case FSC_UNS_STOMIC:
    case FSC_LKD:
    case FSC_UNS_EXCL:
    case FSC_CPR:
    default:
        vmm_info("unsupport fsc 0x%x\n", fsc);
        break;
    }


    return 0;
}

void do_guest_exception(struct cpu_user_regs *regs, int is_compat) {

    if(is_compat) {
        panic("Not support AArch32 Mode\n");
    }
    uint64_t elr = mrs(elr_el1); /* elr_el1 != elr_el2 */
    // uint64_t elr2 = mrs(elr_el2);
    // uint64_t spsr_el1 = mrs(spsr_el1);

    vmm_debug("GUEST excep spsr:%x, elr:%x\n", regs->cpsr, elr);

    const union esr esr = { .bits = regs->esr };

    vmm_info("Exception details: EC:0x%x, ISS:0x%x\n", esr.ec, esr.iss);

    switch(esr.ec) {
        case HSR_EC_INSTR_ABORT_LOWER_EL:
            vmm_info("guest iabt addr:%p\n", elr);
            vmm_info("guest need stage2 mmap here\n");
            vmm_info("gva:%p, ipa:%p\n", get_gva(), get_ipa());
            break;
        case HSR_EC_DATA_ABORT_LOWER_EL:
            vmm_info("guest dabt addr gva:%p, ipa:%p\n", get_gva(), get_ipa());
            vmm_info("guest need stage2 mmap here\n");
            do_stage2_data_abort_trap(regs, esr);
            break;
        default:
            vmm_info("unknown exception\n");

    }
    while(1);
}

void panic(char *msg) {
    printf("panic.........\n");
    vmm_exit(1);

}

void guest_entry(void) {
    /* flush local TLB */
    asm volatile("dsb nshst\n\t"
                 "dsb nsh\n\t"
                 "isb\n\t" ::
                         : "memory");

    uint64_t uart_addr = 0x09000000;
    *(volatile uint64_t*)uart_addr = 'H';
    *(volatile uint64_t*)uart_addr = 'E';
    *(volatile uint64_t*)uart_addr = 'L';
    *(volatile uint64_t*)uart_addr = 'L';
    *(volatile uint64_t*)uart_addr = 'O';

    printf(" from GUEST\n");
    printf("current EL:%d\n", current_el());

    size_t v = 1;
    while (1) {
        asm volatile("mov X3, %0" ::"r"(v) : "cc");
        v++;
    }

}

uint64_t get_default_hcr_flags(void)
{
    return  (HCR_PTW|HCR_BSU_INNER|HCR_AMO|HCR_IMO|HCR_FMO|HCR_VM|
             HCR_TID3|HCR_TSC|HCR_TAC|HCR_SWIO|HCR_TIDCP|HCR_FB|HCR_TSW);
}

void switch_to_el1(void) {
    msr(sctlr_el1, 0);


    // hcr_val &= ~1;//disable vmmu;

    extern void *_guest_stack_end;

    msr(elr_el2, guest_entry);
    msr(sp_el1, &_guest_stack_end);
    msr(spsr_el1, 0);

    //init Generic timer
    // msr cnthctl_el2 , msr cntvoff_el2

    /* init MPID/MPIDR */
    uint64_t tmp = mrs(midr_el1);
    msr(vpidr_el2, tmp);
    tmp = mrs(mpidr_el1);
    msr(vmpidr_el2, tmp);
    vmm_info("vpidr:%x\n", tmp);

    // disable co-processor traps
#if 1
    msr(cptr_el2, (3 << 12 | 0x3ff));
    msr(hstr_el2, 0);
    msr(cpacr_el1, (3 << 20)); /* Enable FP/SIMD at EL1 */

    /*
        SCTLR_EL1 init
    */
    uint64_t sctlr_val =
            (SCTLR_EL1_RES1 | SCTLR_EL1_UCI_DIS | SCTLR_EL1_EE_LE |
                    SCTLR_EL1_WXN_DIS | SCTLR_EL1_NTWE_DIS |
                    SCTLR_EL1_NTWI_DIS | SCTLR_EL1_UCT_DIS | SCTLR_EL1_DZE_DIS |
                    SCTLR_EL1_ICACHE_DIS | SCTLR_EL1_UMA_DIS |
                    SCTLR_EL1_SED_EN | SCTLR_EL1_ITD_EN |
                    SCTLR_EL1_CP15BEN_DIS | SCTLR_EL1_SA0_DIS |
                    SCTLR_EL1_SA_DIS | SCTLR_EL1_DCACHE_DIS |
                    SCTLR_EL1_ALIGN_DIS | SCTLR_EL1_MMU_DIS);
    msr(sctlr_el1, sctlr_val);

#endif
    uint64_t vector_el2 = mrs(vbar_el2);
    msr(vbar_el1, vector_el2);


    tmp = (SPSR_EL_DEBUG_MASK | SPSR_EL_SERR_MASK |\
			SPSR_EL_IRQ_MASK | SPSR_EL_FIQ_MASK |\
			SPSR_EL_M_AARCH64 | SPSR_EL_M_EL1H);
    vmm_debug("SPSR_EL2:%x\n", tmp);
    msr(spsr_el2, 0x3c5);
    asm volatile("eret\t\n":::"memory");
    while(1);
}

void do_hyper_sync(struct cpu_user_regs *regs, int magic) {
    uint64_t esr = mrs(esr_el2);
    uint64_t far = mrs(far_el2);
    uint64_t elr = mrs(elr_el2);
    vmm_info("spsr: 0x%x, esr: %x, far:%lx,elr:%x, lr:%x\n", regs->cpsr, esr,
            far, elr, regs->lr);

    int ec = esr >> 26;
    vmm_info("Exception details: EC:0x%x, ISS:0x%x\n", ec, esr & 0x1ffffff);

    vmm_info("spsr:%x, hcr_el2:%x\n", regs->cpsr, mrs(hcr_el2));
    const union esr esru = {.bits = regs->esr};

    print_iss_detail(esru);
    panic("panic");
}

void on_guest_excep_return(void ){

}