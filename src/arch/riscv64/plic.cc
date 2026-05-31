#include "plic.h"
#include "inline_asm.h"
#include "safe_printf.h"

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
