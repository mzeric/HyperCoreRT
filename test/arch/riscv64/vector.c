#include "htypes.h"
#include "arch_regs.h"
#include "inline_asm.h"
#include "riscv64_system.h"
#include "safe_printf.h"

void panic() {
    safe_printf("Kernel panic\n");
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

    panic();
}


void do_exception(struct cpu_user_regs *regs, u64 cause) {
    const struct fault_info *inf;

    if (cause & (1ul << 63)) {
        cause &= ~(0x1u << 63);
        safe_printf("Guest Interrupt: 0x%x\n", cause);
    }else {
        safe_printf("Guest Trap: 0x%lx\n", cause);
    }

    do_trap_error(regs, "");
}

extern char *__riscv_vector;

void setup_traps() {
    // csrw(CSR_SSCRATCH, 0);
    csrw(stvec, &__riscv_vector);

    csrw(sie, -1);
   	csrs(sstatus, 0x2u);

}