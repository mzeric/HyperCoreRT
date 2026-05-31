/*
 * NS16550A UART emulation for RISC-V hypervisor.
 *
 * Guest accesses UART at GPA 0x20000000. The hypervisor intercepts
 * stage-2 page faults and emulates register reads/writes, forwarding
 * THR writes to the real host UART at 0x10000000.
 */

#include "emul_dev.h"
#include "emul_uart.h"
#include "safe_printf.h"
#include <string.h>

/* NS16550A register offsets */
#define UART_THR  0x00  /* Transmit Holding (write) / Receive Buffer (read) */
#define UART_IER  0x01  /* Interrupt Enable */
#define UART_IIR  0x02  /* Interrupt ID (read) / FCR (write) */
#define UART_LCR  0x03  /* Line Control */
#define UART_MCR  0x04  /* Modem Control */
#define UART_LSR  0x05  /* Line Status */
#define UART_MSR  0x06  /* Modem Status */
#define UART_SCR  0x07  /* Scratch */

/* LSR bits */
#define LSR_DR    0x01  /* Data Ready */
#define LSR_THRE  0x20  /* Transmit Holding Register Empty */
#define LSR_TEMT  0x40  /* Transmitter Empty */

/* Host UART physical address (QEMU virt UART0) */
#define HOST_UART_BASE  0x10000000

struct ns16550a_state {
    u8 ier;
    u8 iir;
    u8 lcr;
    u8 mcr;
    u8 lsr;
    u8 msr;
    u8 scr;
};

static struct ns16550a_state g_uart_state;

static void host_uart_putc(u8 ch) {
    *(volatile int *)HOST_UART_BASE = ch;
}

static int ns16550a_read(struct emul_device *dev, uint64_t addr, int len,
                         uint64_t *value) {
    u32 off = (u32)(addr & 0xFF);

    switch (off) {
    case UART_THR:
        *value = 0; /* No RX data */
        return 0;
    case UART_IER:
        *value = g_uart_state.ier;
        return 0;
    case UART_IIR:
        *value = g_uart_state.iir;
        return 0;
    case UART_LCR:
        *value = g_uart_state.lcr;
        return 0;
    case UART_MCR:
        *value = g_uart_state.mcr;
        return 0;
    case UART_LSR:
        *value = g_uart_state.lsr | LSR_THRE | LSR_TEMT;
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
        host_uart_putc((u8)(value & 0xFF));
        return 0;
    case UART_IER:
        g_uart_state.ier = (u8)value;
        return 0;
    case UART_IIR:  /* FCR on write */
        g_uart_state.iir = (u8)value;
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
    g_uart_state.iir = 0x01;
    register_emul_driver(&ns16550a_driver);
}

/* Stubs — not used on RISC-V but required by emul_uart.h */
int  uart_vcpu_inject_rx(u8 ch)        { return -1; }
void uart_emul_service_host(void)       { }
