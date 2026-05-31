#include "plic.h"
#include "inline_asm.h"
#include "safe_printf.h"
#include "config.h"
#include "emul_dev.h"
#include "exception.h"
#include "riscv_csr.h"
#include "sched.h"
#include "spin_lock.h"
#include <string.h>

static inline void plic_write(u64 addr, u32 val) {
    *(volatile u32 *)addr = val;
}

static inline u32 plic_read(u64 addr) {
    return *(volatile u32 *)addr;
}

static int g_plic_context;

void plic_init(void) {
    int hart = 0; /* Single hart for Phase 3 */
    g_plic_context = PLIC_S_CONTEXT(hart);

    /* Disable all interrupts and set threshold to 0 */
    for (int i = 0; i < PLIC_MAX_IRQ; i += 32) {
        plic_write(PLIC_ENABLE(g_plic_context, i), 0);
    }

    /* Set priority threshold to 0 (accept all priorities) */
    plic_write(PLIC_THRESHOLD(g_plic_context), 0);

    /* Set all priorities to 1 */
    for (int i = 1; i <= PLIC_MAX_IRQ; i++) {
        plic_write(PLIC_PRIORITY(i), 1);
    }

    safe_printf("PLIC init done (context=%d)\n", g_plic_context);
}

void plic_irq_enable(u32 irq) {
    u64 reg = PLIC_ENABLE(g_plic_context, irq);
    u32 bit = 1UL << (irq % 32);
    plic_write(reg, plic_read(reg) | bit);
}

void plic_irq_disable(u32 irq) {
    u64 reg = PLIC_ENABLE(g_plic_context, irq);
    u32 bit = 1UL << (irq % 32);
    plic_write(reg, plic_read(reg) & ~bit);
}

u32 plic_claim(void) {
    return plic_read(PLIC_CLAIM(g_plic_context));
}

void plic_complete(u32 irq) {
    plic_write(PLIC_CLAIM(g_plic_context), irq);
}

#define VPLIC_CONTEXTS CONFIG_SMP_CPU_NUM
#define VPLIC_WORDS    ((PLIC_MAX_IRQ + 31) / 32)

struct vplic_state {
    u32 priority[PLIC_MAX_IRQ + 1];
    u32 pending[VPLIC_WORDS];
    u32 enable[VPLIC_CONTEXTS][VPLIC_WORDS];
    u32 threshold[VPLIC_CONTEXTS];
};

static struct vplic_state g_vplic;
static spinlock_t g_vplic_lock = {.lock = SPIN_UNLOCKED};

static bool vplic_irq_valid(u32 irq) {
    return irq > 0 && irq <= PLIC_MAX_IRQ;
}

static bool vplic_irq_pending_locked(u32 irq) {
    return g_vplic.pending[irq / 32] & (1U << (irq % 32));
}

static bool vplic_irq_enabled_locked(u32 context, u32 irq) {
    if (context >= VPLIC_CONTEXTS)
        return false;
    return g_vplic.enable[context][irq / 32] & (1U << (irq % 32));
}

static bool vplic_irq_deliverable_locked(u32 context, u32 irq) {
    if (!vplic_irq_valid(irq))
        return false;
    if (!vplic_irq_pending_locked(irq) || !vplic_irq_enabled_locked(context, irq))
        return false;
    return g_vplic.priority[irq] > g_vplic.threshold[context];
}

static bool vplic_has_deliverable_locked(void) {
    for (u32 context = 0; context < VPLIC_CONTEXTS; context++) {
        for (u32 irq = 1; irq <= PLIC_MAX_IRQ; irq++) {
            if (vplic_irq_deliverable_locked(context, irq))
                return true;
        }
    }
    return false;
}

static void vplic_set_vseip(bool asserted) {
    hyper_task_t *task = current_task();
    if (!task || !task->vcpu)
        return;

    u64 bit = 1UL << IRQ_VS_EXT;
    if (asserted) {
        task->vcpu->carch.hvip |= bit;
        csrs(CSR_HVIP, bit);
    } else {
        task->vcpu->carch.hvip &= ~bit;
        csrc(CSR_HVIP, bit);
    }
}

void riscv_vplic_refresh(void) {
    arch_spin_lock(&g_vplic_lock);
    bool asserted = vplic_has_deliverable_locked();
    arch_spin_unlock(&g_vplic_lock);
    vplic_set_vseip(asserted);
}

void riscv_vplic_raise(u32 irq) {
    if (!vplic_irq_valid(irq))
        return;

    arch_spin_lock(&g_vplic_lock);
    g_vplic.pending[irq / 32] |= 1U << (irq % 32);
    bool asserted = vplic_has_deliverable_locked();
    arch_spin_unlock(&g_vplic_lock);
    vplic_set_vseip(asserted);
}

void riscv_vplic_clear(u32 irq) {
    if (!vplic_irq_valid(irq))
        return;

    arch_spin_lock(&g_vplic_lock);
    g_vplic.pending[irq / 32] &= ~(1U << (irq % 32));
    bool asserted = vplic_has_deliverable_locked();
    arch_spin_unlock(&g_vplic_lock);
    vplic_set_vseip(asserted);
}

static u32 vplic_claim_locked(u32 context) {
    if (context >= VPLIC_CONTEXTS)
        return 0;

    u32 best_irq = 0;
    u32 best_prio = 0;
    for (u32 irq = 1; irq <= PLIC_MAX_IRQ; irq++) {
        if (!vplic_irq_deliverable_locked(context, irq))
            continue;
        if (g_vplic.priority[irq] > best_prio) {
            best_prio = g_vplic.priority[irq];
            best_irq = irq;
        }
    }

    if (best_irq)
        g_vplic.pending[best_irq / 32] &= ~(1U << (best_irq % 32));
    return best_irq;
}

static int vplic_context_from_claim_off(u32 off) {
    if (off < 0x200000)
        return -1;
    u32 rel = off - 0x200000;
    if ((rel & 0xfff) != 4)
        return -1;
    return rel >> 12;
}

static int vplic_context_from_threshold_off(u32 off) {
    if (off < 0x200000)
        return -1;
    u32 rel = off - 0x200000;
    if ((rel & 0xfff) != 0)
        return -1;
    return rel >> 12;
}

static int vplic_read(struct emul_device *dev, uint64_t addr, int len, uint64_t *value) {
    (void)dev;
    (void)len;
    u32 off = (u32)(addr - PLIC_BASE);
    *value = 0;

    arch_spin_lock(&g_vplic_lock);
    if (off < 0x1000) {
        u32 irq = off / 4;
        if (vplic_irq_valid(irq))
            *value = g_vplic.priority[irq];
    } else if (off >= 0x1000 && off < 0x2000) {
        u32 word = (off - 0x1000) / 4;
        if (word < VPLIC_WORDS)
            *value = g_vplic.pending[word];
    } else if (off >= 0x2000 && off < 0x2000 + VPLIC_CONTEXTS * 0x80) {
        u32 rel = off - 0x2000;
        u32 context = rel / 0x80;
        u32 word = (rel & 0x7f) / 4;
        if (context < VPLIC_CONTEXTS && word < VPLIC_WORDS)
            *value = g_vplic.enable[context][word];
    } else {
        int context = vplic_context_from_threshold_off(off);
        if (context >= 0 && context < VPLIC_CONTEXTS) {
            *value = g_vplic.threshold[context];
        } else {
            context = vplic_context_from_claim_off(off);
            if (context >= 0 && context < VPLIC_CONTEXTS)
                *value = vplic_claim_locked((u32)context);
        }
    }
    bool asserted = vplic_has_deliverable_locked();
    arch_spin_unlock(&g_vplic_lock);

    vplic_set_vseip(asserted);
    return 0;
}

static int vplic_write(struct emul_device *dev, uint64_t addr, int len, uint64_t value) {
    (void)dev;
    (void)len;
    u32 off = (u32)(addr - PLIC_BASE);
    u32 val = (u32)value;

    arch_spin_lock(&g_vplic_lock);
    if (off < 0x1000) {
        u32 irq = off / 4;
        if (vplic_irq_valid(irq))
            g_vplic.priority[irq] = val & PLIC_MAX_PRIORITY;
    } else if (off >= 0x2000 && off < 0x2000 + VPLIC_CONTEXTS * 0x80) {
        u32 rel = off - 0x2000;
        u32 context = rel / 0x80;
        u32 word = (rel & 0x7f) / 4;
        if (context < VPLIC_CONTEXTS && word < VPLIC_WORDS)
            g_vplic.enable[context][word] = val;
    } else {
        int context = vplic_context_from_threshold_off(off);
        if (context >= 0 && context < VPLIC_CONTEXTS) {
            g_vplic.threshold[context] = val;
        } else {
            context = vplic_context_from_claim_off(off);
            (void)context; /* Completion is edge-free for the simple in-memory PLIC. */
        }
    }
    bool asserted = vplic_has_deliverable_locked();
    arch_spin_unlock(&g_vplic_lock);

    vplic_set_vseip(asserted);
    return 0;
}

static struct emul_driver_ops vplic_ops = {
    .read = vplic_read,
    .write = vplic_write,
};

static struct emul_driver vplic_driver = {
    .name = (char *)"riscv-plic",
    .ops = &vplic_ops,
};

void plic_register_emul(void) {
    memset(&g_vplic, 0, sizeof(g_vplic));
    for (u32 irq = 1; irq <= PLIC_MAX_IRQ; irq++)
        g_vplic.priority[irq] = 1;
    register_emul_driver(&vplic_driver);
}
