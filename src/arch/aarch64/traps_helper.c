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

#include <stdio.h> /* just remove guest warning */
#include <string.h>
#include <ioremap.h>
#include <guest_memory.h>



void print_iss_detail(const union esr esr) {
    int fsc = esr.dabt.fsc;

    switch (fsc) {
    case 0 ... 3:
        hyper_info("FSC: address size fault level:%d", fsc & FSC_LL_MASK);
        break;
    case FSC_FLT_TRANS ... FSC_FLT_TRANS + 3:
        hyper_info("FSC: translation fault level:%d", fsc & FSC_LL_MASK);
        break;
    case FSC_FLT_ACCESS ... FSC_FLT_ACCESS + 3:
        hyper_info("FSC: access flag fault level:%d", fsc & FSC_LL_MASK);
        break;
    case FSC_FLT_PERM ... FSC_FLT_PERM + 3:
        hyper_info("FSC: permission fault level:%d", fsc & FSC_LL_MASK);
        break;
    case FSC_SEA:
        hyper_info("FSC: Synchronous external abort");
        break;
    case FSC_SPE:
        hyper_info("FSC: Synchronous parity or ECC error on memory access, "
                 "not on translation table walk");
        break;
    case FSC_APE:
        hyper_info("FSC:FSC_APE");
        break;
    case FSC_SEATT ... FSC_SEATT + 3:
        hyper_info("FSC: Synchronous external abort, on translation table "
                 "walk, level:%d",
                fsc & FSC_LL_MASK);
        break;
    case FSC_SPETT ... FSC_SPETT + 3:
        hyper_info("FSC: Synchronous parity or ECC error on memory access on "
                 "translation table walk, level:%d",
                fsc & FSC_LL_MASK);
        break;

    case FSC_AF:
        hyper_info("FSC: Alignment fault");
        break;
    case FSC_TLB_FLT:
        hyper_info("FSC: TLB conflict abort");
        break;
    case FSC_UNS_STOMIC:
    case FSC_LKD:
    case FSC_UNS_EXCL:
    case FSC_CPR:
    default:
        hyper_info("unsupport fsc 0x%x", fsc);
        break;
    }


    return;
}

void guest_entry(void) {
    /* flush local TLB */
    asm volatile("dsb nshst\n\t"
                 "dsb nsh\n\t"
                 "isb\n\t" ::
                         : "memory");

    safe_printf("Hello from GUEST\n");
    safe_printf("current EL:%d\n", current_el());

    while (1){
        wfi();
        safe_printf("guest wakeup\n");
    }
}

void switch_to_el1(void) {
    msr(sctlr_el1, 0);


    // hcr_val &= ~1;//disable vmmu;

    extern void *_guest_stack_end;

    hyper_info("el1 sp:%p", &_guest_stack_end);

    msr(elr_el2, guest_entry);
    msr(sp_el1, &_guest_stack_end);
    msr(spsr_el1, 0);

    //init Generic timer
    // msr cnthctl_el2 , msr cntvoff_el2

    /* init MPID/MPIDR */
    uint64_t tmp = mrs(midr_el1);
#if 0
    msr(vpidr_el2, tmp);
    tmp = mrs(mpidr_el1);
    msr(vmpidr_el2, tmp);
    hyper_info("vpidr:%x", tmp);

    // disable co-processor traps
    // msr(cptr_el2, (3 << 12 | 0x3ff));
    msr(cptr_el2, 0);
    msr(hstr_el2, 0);
#endif
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
    hyper_info("sctlr_el1: %lx", sctlr_val);

    // uint64_t vector_el2 = mrs(vbar_el2);
    // msr(vbar_el1, vector_el2);


    tmp = (SPSR_EL_DEBUG_MASK | SPSR_EL_SERR_MASK |\
			SPSR_EL_IRQ_MASK | SPSR_EL_FIQ_MASK |\
			SPSR_EL_M_AARCH64 | SPSR_EL_M_EL1H);
    hyper_debug("SPSR_EL2:%lx", tmp);
    msr(spsr_el2, 0x3c5);
    asm volatile("eret\t\n":::"memory");
    while (1)
        ;
}



uint64_t get_default_hcr_flags(void)
{
    return  (HCR_PTW|HCR_BSU_INNER|HCR_AMO|HCR_IMO|HCR_FMO|HCR_VM|
             HCR_TSC|HCR_TAC|HCR_SWIO|HCR_TIDCP|HCR_FB|HCR_TSW);
}

void panic(char *msg) {
    safe_printf("panic:%s .........\n", msg);
    hyper_exit(1);

}

