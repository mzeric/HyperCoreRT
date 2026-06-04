#include "riscv_sbi_manager.h"

#include "exception.h"
#include "guest_dtb.h"
#include "inline_asm.h"
#include "ipi.h"
#include "mm.h"
#include "mmu.h"
#include "riscv_features.h"
#include "riscv_fence_manager.h"
#include "riscv_sbi.h"
#include "riscv_timer_manager.h"
#include "riscv_virt_irq_manager.h"
#include "safe_printf.h"
#include "sbi_helper.h"
#include "sched.h"

void hfence(void);

#define SATP_MODE_SHIFT 60
#define SATP_PPN_MASK   ((1UL << 44) - 1)
#define PTE_V           (1UL << 0)
#define PTE_R           (1UL << 1)
#define PTE_W           (1UL << 2)
#define PTE_X           (1UL << 3)
#define PTE_PPN_MASK    ((1UL << 44) - 1)

static vcpu_t *riscv_current_vcpu(void) {
    hyper_task_t *task = current_task();
    return task ? task->vcpu : NULL;
}

static bool guest_va_to_pa(u64 va, u64 *pa) {
    u64 satp = csrr(CSR_VSATP);
    u64 mode = satp >> SATP_MODE_SHIFT;
    if (mode == SATP_MODE_OFF) {
        *pa = va;
        return true;
    }

    int top_level;
    if (mode == SATP_MODE_SV39)
        top_level = 2;
    else if (mode == SATP_MODE_SV48)
        top_level = 3;
    else
        return false;

    u64 table_pa = (satp & SATP_PPN_MASK) << PAGE_SHIFT;
    for (int level = top_level; level >= 0; level--) {
        u64 idx = (va >> (PAGE_SHIFT + PAGE_LEVEL_WIDTH * level)) & 0x1ff;
        ptw_t *table = (ptw_t *)phy_to_vir(table_pa);
        ptw_t pte = table[idx];

        if (!(pte.bits & PTE_V) || ((pte.bits & PTE_W) && !(pte.bits & PTE_R)))
            return false;

        if (pte.bits & (PTE_R | PTE_X)) {
            u64 page_off_bits = PAGE_SHIFT + PAGE_LEVEL_WIDTH * level;
            u64 page_off_mask = (1UL << page_off_bits) - 1;
            *pa = (((pte.bits >> 10) & PTE_PPN_MASK) << PAGE_SHIFT) |
                  (va & page_off_mask);
            return true;
        }

        table_pa = ((pte.bits >> 10) & PTE_PPN_MASK) << PAGE_SHIFT;
    }
    return false;
}

static bool guest_read_u64_va(u64 va, u64 *value) {
    u64 v = 0;
    for (int i = 0; i < 8; i++) {
        u64 pa;
        if (!guest_va_to_pa(va + i, &pa))
            return false;
        v |= ((u64)*(u8 *)phy_to_vir(pa)) << (i * 8);
    }
    *value = v;
    return true;
}

static void inject_guest_soft_irq(u64 hartid) {
    hyper_task_t *task = riscv_find_guest_vcpu(hartid);
    if (!task || !task->vcpu)
        return;

    riscv_virt_irq_assert(task->vcpu, IRQ_VS_SOFT);
    if (task != current_task() && task->pcpu_affinity >= 0)
        ipi_send_reschedule(task->pcpu_affinity);
}

static void clear_guest_soft_irq(void) {
    vcpu_t *vcpu = riscv_current_vcpu();
    if (vcpu)
        riscv_virt_irq_clear(vcpu, IRQ_VS_SOFT);
}

static void handle_legacy_send_ipi(struct cpu_user_regs *args) {
    u64 mask;
    if (args->a0 == 0) {
        mask = (riscv_guest_vcpu_count() >= 64) ? ~0UL : ((1UL << riscv_guest_vcpu_count()) - 1);
    } else if (!guest_read_u64_va(args->a0, &mask)) {
        safe_printf("legacy send_ipi: failed to read guest mask at %lx\n", args->a0);
        args->a0 = 0;
        return;
    }

    for (u32 hart = 0; hart < riscv_guest_vcpu_count() && hart < 64; hart++) {
        if (mask & (1UL << hart))
            inject_guest_soft_irq(hart);
    }
    args->a0 = 0;
}

static void handle_sbi_send_ipi(struct cpu_user_regs *args) {
    u64 mask = args->a0;
    u64 base = args->a1;
    u32 count = riscv_guest_vcpu_count();

    if (base == (u64)-1) {
        for (u32 hart = 0; hart < count; hart++)
            inject_guest_soft_irq(hart);
    } else {
        for (u32 bit = 0; bit < 64; bit++) {
            if (!(mask & (1UL << bit)))
                continue;
            u64 hart = base + bit;
            if (hart < count)
                inject_guest_soft_irq(hart);
        }
    }

    args->a0 = 0;
    args->a1 = 0;
}

static long handle_legacy_remote_fence(struct cpu_user_regs *args, enum riscv_fence_kind kind) {
    u64 mask;
    if (args->a0 == 0) {
        mask = (riscv_guest_vcpu_count() >= 64) ? ~0UL : ((1UL << riscv_guest_vcpu_count()) - 1);
    } else if (!guest_read_u64_va(args->a0, &mask)) {
        safe_printf("legacy remote fence: failed to read guest mask at %lx\n", args->a0);
        return SBI_ERR_INVALID_ADDRESS;
    }

    return riscv_remote_fence(kind, mask, 0);
}

static bool rfence_kind_from_fid(u64 fid, enum riscv_fence_kind *kind) {
    switch (fid) {
    case SBI_EXT_RFENCE_REMOTE_FENCE_I:
        *kind = RISCV_FENCE_REMOTE_FENCE_I;
        return true;
    case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA:
        *kind = RISCV_FENCE_REMOTE_HFENCE_GVMA;
        return true;
    case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA_VMID:
        *kind = RISCV_FENCE_REMOTE_HFENCE_GVMA_VMID;
        return true;
    case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA_ASID:
    case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID:
        *kind = RISCV_FENCE_REMOTE_HFENCE_VVMA_ASID;
        return true;
    case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA:
    case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA:
        *kind = RISCV_FENCE_REMOTE_HFENCE_VVMA;
        return true;
    default:
        return false;
    }
}

bool riscv_sbi_extension_supported(u64 extension_id) {
    switch (extension_id) {
    case SBI_EXT_BASE:
    case SBI_EXT_TIME:
    case SBI_EXT_IPI:
    case SBI_EXT_RFENCE:
        return true;
    case SBI_EXT_HSM:
    default:
        return false;
    }
}

struct RiscvSbiResult riscv_sbi_not_supported(void) {
    struct RiscvSbiResult result = {
        .error = SBI_ERR_NOT_SUPPORTED,
        .value = 0,
    };
    return result;
}

static void handle_base_extension(struct cpu_user_regs *args, u64 fid) {
    if (fid == SBI_EXT_BASE_PROBE_EXT) {
        args->a1 = riscv_sbi_extension_supported(args->a0) ? 1 : 0;
        args->a0 = SBI_SUCCESS;
        return;
    }

    struct sbiret ret = sbi_ecall(SBI_EXT_BASE, fid, args->a0, args->a1,
                                  args->a2, args->a3, args->a4, args->a5);
    args->a0 = (u64)ret.error;
    args->a1 = (u64)ret.value;
}

void riscv_sbi_handle_vs_ecall(struct cpu_user_regs *args) {
    u64 ext = args->a7;
    u64 fid = args->a6;

    switch (ext) {
    case SBI_EXT_0_1_CONSOLE_PUTCHAR:
        safe_printf("%c", (char)args->a0);
        args->a0 = SBI_SUCCESS;
        break;
    case SBI_EXT_0_1_SHUTDOWN:
        safe_printf("guest shutdown requested\n");
        args->a0 = SBI_SUCCESS;
        break;
    case SBI_EXT_0_1_SET_TIMER:
        riscv_vcpu_timer_arm_current(args->a0);
        args->a0 = SBI_SUCCESS;
        break;
    case SBI_EXT_0_1_CLEAR_IPI:
        clear_guest_soft_irq();
        args->a0 = SBI_SUCCESS;
        break;
    case SBI_EXT_0_1_SEND_IPI:
        handle_legacy_send_ipi(args);
        break;
    case SBI_EXT_0_1_REMOTE_FENCE_I:
        args->a0 = handle_legacy_remote_fence(args, RISCV_FENCE_REMOTE_FENCE_I);
        break;
    case SBI_EXT_0_1_REMOTE_SFENCE_VMA:
        args->a0 = handle_legacy_remote_fence(args, RISCV_FENCE_REMOTE_HFENCE_VVMA);
        break;
    case SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID:
        args->a0 = handle_legacy_remote_fence(args, RISCV_FENCE_REMOTE_HFENCE_VVMA_ASID);
        break;
    case SBI_EXT_BASE:
        handle_base_extension(args, fid);
        break;
    case SBI_EXT_IPI:
        if (fid == SBI_EXT_IPI_SEND_IPI) {
            handle_sbi_send_ipi(args);
            break;
        }
        args->a0 = (u64)SBI_ERR_NOT_SUPPORTED;
        args->a1 = 0;
        break;
    case SBI_EXT_RFENCE:
    {
        enum riscv_fence_kind kind;
        if (!rfence_kind_from_fid(fid, &kind)) {
            args->a0 = (u64)SBI_ERR_NOT_SUPPORTED;
            args->a1 = 0;
            break;
        }
        args->a0 = (u64)riscv_remote_fence(kind, args->a0, args->a1);
        args->a1 = 0;
        break;
    }
    case SBI_EXT_TIME:
        if (fid == SBI_EXT_TIME_SET_TIMER) {
            riscv_vcpu_timer_arm_current(args->a0);
            args->a0 = SBI_SUCCESS;
            args->a1 = 0;
            break;
        }
        args->a0 = (u64)SBI_ERR_NOT_SUPPORTED;
        args->a1 = 0;
        break;
    case SBI_EXT_HSM:
        args->a0 = (u64)SBI_ERR_NOT_SUPPORTED;
        args->a1 = 0;
        break;
    default:
        safe_printf("VS-ecall: ext=%lx fid=%lu\n", ext, fid);
        args->a0 = (u64)-1;
        args->a1 = 0;
        break;
    }
    args->sepc += 4;
}

void riscv_handle_vs_ecall(struct cpu_user_regs *args) {
    riscv_sbi_handle_vs_ecall(args);
}
