/*
 * NS16550A UART emulation for RISC-V hypervisor.
 *
 * Guest accesses UART at GPA 0x20000000. The hypervisor intercepts
 * stage-2 page faults and emulates register reads/writes, forwarding
 * THR writes to the real host UART at 0x10000000.
 */

#include "emul_dev.h"
#include "emul_uart.h"
#include "plic.h"
#include "safe_printf.h"
#include "spin_lock.h"
#include <string.h>

#define UART_EMUL_FIFO_SIZE 128U

/* NS16550A register offsets */
#define UART_THR  0x00  /* Transmit Holding (write) / Receive Buffer (read) */
#define UART_IER  0x01  /* Interrupt Enable */
#define UART_IIR  0x02  /* Interrupt ID (read) / FCR (write) */
#define UART_LCR  0x03  /* Line Control */
#define UART_MCR  0x04  /* Modem Control */
#define UART_LSR  0x05  /* Line Status */
#define UART_MSR  0x06  /* Modem Status */
#define UART_SCR  0x07  /* Scratch */

/* IER bits */
#define IER_RDI   0x01  /* Received Data Available */
#define IER_THRI  0x02  /* Transmitter Holding Register Empty */

/* IIR values */
#define IIR_NO_INT 0x01
#define IIR_THRI   0x02
#define IIR_RDI    0x04

/* LCR bits */
#define LCR_DLAB  0x80

/* LSR bits */
#define LSR_DR    0x01  /* Data Ready */
#define LSR_THRE  0x20  /* Transmit Holding Register Empty */
#define LSR_TEMT  0x40  /* Transmitter Empty */

/* Host UART physical address (QEMU virt UART0) */
#define HOST_UART_BASE  0x10000000
#define HOST_UART_RBR   (HOST_UART_BASE + 0x00)
#define HOST_UART_LSR   (HOST_UART_BASE + 0x05)

#define GUEST_UART_IRQ  10

struct ns16550a_state {
    u8 rx_fifo[UART_EMUL_FIFO_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u32 rx_count;
    u8 dll;
    u8 dlm;
    u8 ier;
    u8 iir;
    u8 fcr;
    u8 lcr;
    u8 mcr;
    u8 lsr;
    u8 msr;
    u8 scr;
    bool tx_irq_pending;
};

static struct ns16550a_state g_uart_state;
static spinlock_t g_uart_lock = {.lock = SPIN_UNLOCKED};

static void host_uart_putc(u8 ch) {
    *(volatile int *)HOST_UART_BASE = ch;
}

static bool ns16550a_rx_empty(void) {
    return g_uart_state.rx_count == 0;
}

static bool ns16550a_rx_full(void) {
    return g_uart_state.rx_count == UART_EMUL_FIFO_SIZE;
}

static int ns16550a_rx_pop(u8 *ch) {
    if (ns16550a_rx_empty())
        return -1;

    *ch = g_uart_state.rx_fifo[g_uart_state.rx_tail];
    g_uart_state.rx_tail = (g_uart_state.rx_tail + 1) % UART_EMUL_FIFO_SIZE;
    g_uart_state.rx_count--;
    return 0;
}

static int ns16550a_rx_push(u8 ch) {
    if (ns16550a_rx_full())
        return -1;

    g_uart_state.rx_fifo[g_uart_state.rx_head] = ch;
    g_uart_state.rx_head = (g_uart_state.rx_head + 1) % UART_EMUL_FIFO_SIZE;
    g_uart_state.rx_count++;
    return 0;
}

static void ns16550a_update_irq(void) {
    bool raise = false;

    arch_spin_lock(&g_uart_lock);
    if (!ns16550a_rx_empty() && (g_uart_state.ier & IER_RDI)) {
        g_uart_state.iir = IIR_RDI;
        raise = true;
    } else if (g_uart_state.tx_irq_pending && (g_uart_state.ier & IER_THRI)) {
        g_uart_state.iir = IIR_THRI;
        raise = true;
    } else {
        g_uart_state.iir = IIR_NO_INT;
    }
    arch_spin_unlock(&g_uart_lock);

    if (raise)
        riscv_vplic_raise(GUEST_UART_IRQ);
    else
        riscv_vplic_clear(GUEST_UART_IRQ);
}

static int ns16550a_read(struct emul_device *dev, uint64_t addr, int len,
                         uint64_t *value) {
    u32 off = (u32)(addr & 0xFF);
    u8 ch = 0;

    switch (off) {
    case UART_THR:
        if (g_uart_state.lcr & LCR_DLAB) {
            *value = g_uart_state.dll;
            return 0;
        }
        arch_spin_lock(&g_uart_lock);
        if (ns16550a_rx_pop(&ch) == 0)
            *value = ch;
        else
            *value = 0;
        arch_spin_unlock(&g_uart_lock);
        ns16550a_update_irq();
        return 0;
    case UART_IER:
        if (g_uart_state.lcr & LCR_DLAB) {
            *value = g_uart_state.dlm;
            return 0;
        }
        *value = g_uart_state.ier;
        return 0;
    case UART_IIR:
        if (g_uart_state.iir == IIR_THRI)
            g_uart_state.tx_irq_pending = false;
        *value = g_uart_state.iir;
        ns16550a_update_irq();
        return 0;
    case UART_LCR:
        *value = g_uart_state.lcr;
        return 0;
    case UART_MCR:
        *value = g_uart_state.mcr;
        return 0;
    case UART_LSR:
        *value = g_uart_state.lsr | LSR_THRE | LSR_TEMT;
        if (!ns16550a_rx_empty())
            *value |= LSR_DR;
        return 0;
    case UART_MSR:
        *value = g_uart_state.msr;
        return 0;
    case UART_SCR:
        *value = g_uart_state.scr;
        return 0;
    default:
        *value = 0;
        return 0;
    }
}

static int ns16550a_write(struct emul_device *dev, uint64_t addr, int len,
                          uint64_t value) {
    u32 off = (u32)(addr & 0xFF);

    switch (off) {
    case UART_THR:
        if (g_uart_state.lcr & LCR_DLAB) {
            g_uart_state.dll = (u8)value;
            return 0;
        }
        host_uart_putc((u8)(value & 0xFF));
        g_uart_state.tx_irq_pending = true;
        ns16550a_update_irq();
        return 0;
    case UART_IER:
        if (g_uart_state.lcr & LCR_DLAB) {
            g_uart_state.dlm = (u8)value;
            return 0;
        }
        g_uart_state.ier = (u8)value;
        if (g_uart_state.ier & IER_THRI)
            g_uart_state.tx_irq_pending = true;
        ns16550a_update_irq();
        return 0;
    case UART_IIR:  /* FCR on write */
        g_uart_state.fcr = (u8)value;
        if (value & 0x02) {
            arch_spin_lock(&g_uart_lock);
            g_uart_state.rx_head = 0;
            g_uart_state.rx_tail = 0;
            g_uart_state.rx_count = 0;
            arch_spin_unlock(&g_uart_lock);
        }
        ns16550a_update_irq();
        return 0;
    case UART_LCR:
        g_uart_state.lcr = (u8)value;
        return 0;
    case UART_MCR:
        g_uart_state.mcr = (u8)value;
        return 0;
    case UART_LSR:
        /* LSR is read-only */
        return 0;
    case UART_MSR:
        /* MSR is read-only */
        return 0;
    case UART_SCR:
        g_uart_state.scr = (u8)value;
        return 0;
    default:
        return 0;
    }
}

static struct emul_driver_ops ns16550a_ops = {
    .read  = ns16550a_read,
    .write = ns16550a_write,
};

static struct emul_driver ns16550a_driver = {
    .name = "ns16550a",
    .ops  = &ns16550a_ops,
};

void uart_register_emul(void) {
    memset(&g_uart_state, 0, sizeof(g_uart_state));
    /* IIR: no interrupt pending */
    g_uart_state.iir = IIR_NO_INT;
    g_uart_state.lsr = LSR_THRE | LSR_TEMT;
    register_emul_driver(&ns16550a_driver);
}

int uart_vcpu_inject_rx(u8 ch) {
    int ret;

    arch_spin_lock(&g_uart_lock);
    ret = ns16550a_rx_push(ch);
    arch_spin_unlock(&g_uart_lock);

    ns16550a_update_irq();
    return ret;
}

void uart_emul_service_host(void) {
    volatile u8 *rbr = (volatile u8 *)HOST_UART_RBR;
    volatile u8 *lsr = (volatile u8 *)HOST_UART_LSR;

    while (*lsr & LSR_DR) {
        if (uart_vcpu_inject_rx(*rbr) != 0)
            break;
    }
}
