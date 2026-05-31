#pragma once
#include "htypes.h"

/* QEMU virt PLIC constants */
#define PLIC_BASE           0x0C000000UL
#define PLIC_MAX_IRQ        128
#define PLIC_MAX_PRIORITY   7
#define PLIC_NUM_CONTEXTS   2  /* M-mode + S-mode for hart 0 */

/* Register offsets */
#define PLIC_PRIORITY(irq)      (PLIC_BASE + 0x0000 + ((irq) * 4))
#define PLIC_PENDING(irq)       (PLIC_BASE + 0x1000 + (((irq) / 32) * 4))
#define PLIC_ENABLE(context, irq) \
    (PLIC_BASE + 0x2000 + ((context) * 0x80) + (((irq) / 32) * 4))
#define PLIC_THRESHOLD(context) (PLIC_BASE + 0x200000 + ((context) * 0x1000))
#define PLIC_CLAIM(context)     (PLIC_BASE + 0x200004 + ((context) * 0x1000))

/* S-mode context offset: hart_id * 2 + 1 */
#define PLIC_S_CONTEXT(hart)    ((hart) * 2 + 1)

void plic_init(void);
void plic_irq_enable(u32 irq);
void plic_irq_disable(u32 irq);
u32  plic_claim(void);
void plic_complete(u32 irq);

void plic_register_emul(void);
void riscv_vplic_raise(u32 irq);
void riscv_vplic_clear(u32 irq);
void riscv_vplic_refresh(void);
