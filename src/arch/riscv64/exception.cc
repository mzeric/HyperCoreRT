#include "inline_asm.h"
#include "safe_printf.h"
#include "htypes.h"
#include "include/arch_regs.h"
#include "timer.h"
#include "sched.h"
#include "mm.h"
#include "mmu.h"
#include "exception.h"
#include "guest_memory.h"
#include "emul_dev.h"
#include "emulate.h"
#include "plic.h"
#include "sbi_helper.h"
#include "riscv_sbi.h"
#include "ipi.h"
#include "smp.h"

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
int is_trap_from_guest(u64 hstatus) { return (hstatus & HSTATUS_SPV); }

int do_page_fault(struct cpu_user_regs *regs, const char *str) {
    do_trap_error(regs, str);
    return 0;
}

static void handle_virt_instruction(struct cpu_user_regs *regs) {
    u64 inst = csrr(CSR_STVAL);

    if ((inst & INSN_MASK_WFI) == INSN_MATCH_WFI) {
        regs->sepc += 4;
        return;
    }

    do_trap_error(regs, "unhandled virtual instruction");
}

struct interrupt_info {
    int irq;
    const char *brief;
};

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
    {do_trap_unknown, "guest instruction page fault"}, // 0x14
    {do_trap_unknown, "guest load access fault"},      // 0x15
    {do_trap_unknown, "guest virt instruction fault"},  // 0x16
    {do_trap_unknown, "guest store AMO access fault"},  // 0x17
};

static inline const struct fault_info *ec_to_fault_info(unsigned int scause) {
    return fault_info + (scause & 0x1F);
}

void hfence(void);

static vcpu_t *get_current_vcpu(void) {
    hyper_task_t *task = current_task();
    return task ? task->vcpu : NULL;
}

void do_stage2_fault(struct cpu_user_regs *regs, int cause) {
    u64 htval = csrr(CSR_HTVAL);
    u64 stval = csrr(CSR_STVAL);

    u64 fault_addr = (htval << 2) | (stval & 3);
    fault_addr &= ~(PAGE_SIZE - 1);

    int is_write = (cause == RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT);

    vcpu_t *vcpu = get_current_vcpu();
    if (!vcpu) {
        safe_printf("stage2 fault: no vcpu, addr=%lx cause=%d\n", fault_addr, cause);
        panic("stage2 fault without vcpu");
    }

    struct mem_region *region = guest_mem_find_region(vcpu, fault_addr, 0);
    if (!region) {
        safe_printf("stage2 fault: unmapped addr=%lx cause=%d\n", fault_addr, cause);
        panic("stage2 fault: unknown guest address");
    }

    if (region->dev) {
        /* MMIO device — emulate */
        vcpu_emulate_mmio(vcpu, regs, fault_addr, is_write);
    } else {
        /* RAM — lazy stage-2 map */
        u64 offset = fault_addr - region->gpa;
        u64 hpa = region->hpa + offset;
        pg_map_stage2(fault_addr, hpa, PAGE_SIZE, region->attr, 0);
        hfence();
    }
}

/* SBI extension IDs for passthrough */
#define SBI_EXT_BASE        0x10
#define SBI_EXT_TIME        0x54494D45
#define SBI_EXT_IPI         0x735049
#define SBI_EXT_RFENCE      0x52464E43
#define SBI_EXT_HSM         0x48534D

static void handle_s_ext_irq(void) {
    u32 irq = plic_claim();
    if (irq == 0)
        return;

    safe_printf("PLIC irq: %u\n", irq);
    plic_complete(irq);
}

static void handle_s_soft_irq(void) {
    /* Clear the software interrupt pending bit */
    csrc(sip, (1UL << IRQ_S_SOFT));

    /* Dispatch pending IPIs */
    int cpu = cpu_id();
    extern u32 ipi_get_pending(int);
    extern void ipi_clear_pending(int);
    u32 pending = ipi_get_pending(cpu);
    ipi_clear_pending(cpu);

    for (int vec = 0; vec < IPI_MAX && pending; vec++, pending >>= 1) {
        if (pending & 1)
            ipi_handle(vec);
    }
}

static void handle_vs_ecall(struct cpu_user_regs *args) {
    u64 ext = args->a7;
    u64 fid = args->a6;

    switch (ext) {
    case SBI_EXT_0_1_CONSOLE_PUTCHAR:
        safe_printf("%c", (char)args->a0);
        args->a0 = 0;
        break;
    case SBI_EXT_0_1_SHUTDOWN:
        panic("guest shutdown");
        break;
    case SBI_EXT_0_1_SET_TIMER:
        sbi_set_timer(args->a0);
        args->a0 = 0;
        break;
    case SBI_EXT_0_1_SEND_IPI:
        sbi_send_ipi((const unsigned long *)args->a0);
        args->a0 = 0;
        break;
    case SBI_EXT_BASE:
    case SBI_EXT_TIME:
    case SBI_EXT_IPI:
    case SBI_EXT_RFENCE:
    case SBI_EXT_HSM: {
        /* Forward to host SBI */
        struct sbiret ret = sbi_ecall(ext, fid, args->a0, args->a1,
                                       args->a2, args->a3, args->a4, args->a5);
        args->a0 = (u64)ret.error;
        args->a1 = (u64)ret.value;
        break;
    }
    default:
        safe_printf("VS-ecall: ext=%lx fid=%lu\n", ext, fid);
        args->a0 = (u64)-1;
        args->a1 = 0;
        break;
    }
    args->sepc += 4;
}

extern "C" void do_exception(struct cpu_user_regs *args, u64 cause) {
    const struct fault_info *inf;

    if (cause & (1ul << 63)) {
        cause &= ~(1UL << 63);
        switch (cause) {
        case IRQ_S_SOFT:
            handle_s_soft_irq();
            break;
        case IRQ_S_TIMER:
            handle_timer_irq();
            sched_yield(args);
            break;
        case IRQ_S_EXT:
            handle_s_ext_irq();
            break;
        default:
            safe_printf("unknown irq: %lu\n", cause);
            break;
        }
    } else {
        switch (cause) {
        case RISCV_EXCP_VS_ECALL:
            handle_vs_ecall(args);
            break;
        case RISCV_EXCP_INST_GUEST_PAGE_FAULT:
        case RISCV_EXCP_LOAD_GUEST_ACCESS_FAULT:
        case RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT:
            do_stage2_fault(args, cause);
            break;
        case RISCV_EXCP_VIRT_INSTRUCTION_FAULT:
            handle_virt_instruction(args);
            break;
        default:
            inf = ec_to_fault_info(cause);
            if (!inf->fn(args, inf->name))
                do_trap_error(args, "");
        }
    }
}

void setup_exception(void*entry) {
    csrw(stvec, entry);
    csrs(sstatus, SSTATUS_SIE);
}