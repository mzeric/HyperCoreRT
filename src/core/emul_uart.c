#include "emul_uart.h"
#include "hyper_config.h"
#include "sched.h"
#include "emul_dev.h"
#include "emul_gic.h"
#include "src/drivers/pl011/pl011.h"

#include <string.h>

#define UART_EMUL_FIFO_SIZE 128U

#define UART_INT_RX     (1U << 4)
#define UART_INT_TX     (1U << 5)
#define UART_INT_RT     (1U << 6)
#define UART_INT_ALL    0x7ffU

#define UART_PID4       0xfd0
#define UART_PID5       0xfd4
#define UART_PID6       0xfd8
#define UART_PID7       0xfdc
#define UART_PID0       0xfe0
#define UART_PID1       0xfe4
#define UART_PID2       0xfe8
#define UART_PID3       0xfec
#define UART_CID0       0xff0
#define UART_CID1       0xff4
#define UART_CID2       0xff8
#define UART_CID3       0xffc

struct uart_emul_state {
    u8 rx_fifo[UART_EMUL_FIFO_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u32 rx_count;
    u32 ibrd;
    u32 fbrd;
    u32 lcr_h;
    u32 cr;
    u32 ifls;
    u32 imsc;
    u32 ris;
    u32 dmacr;
    int irq_latched;
};

static struct uart_emul_state g_uart_emul;

static u32 uart_emul_offset(u64 addr)
{
    return addr - hyper_config()->uart.guest_base;
}

static int uart_emul_rx_empty(void)
{
    return g_uart_emul.rx_count == 0;
}

static int uart_emul_rx_full(void)
{
    return g_uart_emul.rx_count == UART_EMUL_FIFO_SIZE;
}

static int uart_emul_rx_pop(u8 *ch)
{
    if (uart_emul_rx_empty())
        return -1;
    *ch = g_uart_emul.rx_fifo[g_uart_emul.rx_tail];
    g_uart_emul.rx_tail = (g_uart_emul.rx_tail + 1) % UART_EMUL_FIFO_SIZE;
    g_uart_emul.rx_count--;
    return 0;
}

static hyper_task_t *uart_emul_target_task(void)
{
    struct hyper_config *cfg = hyper_config();
    hyper_task_t *target = find_task_by_mpidr(cfg->guest.vcpu_mpidr[0]);

    if (!target)
        target = current_task();
    return target;
}

static void uart_emul_refresh_rx_irq(void)
{
    if (uart_emul_rx_empty())
        g_uart_emul.ris &= ~(UART_INT_RX | UART_INT_RT);
    else
        g_uart_emul.ris |= UART_INT_RX | UART_INT_RT;
}

static void uart_emul_update_irq(void)
{
    struct hyper_config *cfg = hyper_config();
    hyper_task_t *target;

    uart_emul_refresh_rx_irq();
    if (!(g_uart_emul.ris & g_uart_emul.imsc & (UART_INT_RX | UART_INT_RT))) {
        g_uart_emul.irq_latched = 0;
        return;
    }

    if (g_uart_emul.irq_latched)
        return;

    target = uart_emul_target_task();
    if (!target)
        return;

    g_uart_emul.irq_latched = 1;
    gic_vcpu_inject_virq(target, cfg->uart.guest_irq);
    if (target == current_task())
        gic_vcpu_flush_lr(target);
}

static u32 uart_emul_read_fr(void)
{
    u32 fr = PL011_UARTFR_TXFE | PL011_UARTFR_CTS;

    if (uart_emul_rx_empty())
        fr |= PL011_UARTFR_RXFE;
    if (uart_emul_rx_full())
        fr |= PL011_UARTFR_RXFF;
    return fr;
}

static u32 uart_emul_read_id(u32 off)
{
    switch (off) {
    case UART_PID4: return 0x04;
    case UART_PID5: return 0x00;
    case UART_PID6: return 0x00;
    case UART_PID7: return 0x00;
    case UART_PID0: return 0x11;
    case UART_PID1: return 0x10;
    case UART_PID2: return 0x14;
    case UART_PID3: return 0x00;
    case UART_CID0: return 0x0d;
    case UART_CID1: return 0xf0;
    case UART_CID2: return 0x05;
    case UART_CID3: return 0xb1;
    default: return 0;
    }
}

static void uart_emul_putc(u8 ch)
{
    volatile u32 *dr = (volatile u32 *)hyper_config()->uart.host_base;

    *dr = ch;
}

int uart_vcpu_inject_rx(u8 ch)
{
    if (!hyper_config()->uart.enabled)
        return -1;
    if (uart_emul_rx_full())
        return -1;

    g_uart_emul.rx_fifo[g_uart_emul.rx_head] = ch;
    g_uart_emul.rx_head = (g_uart_emul.rx_head + 1) % UART_EMUL_FIFO_SIZE;
    g_uart_emul.rx_count++;
    uart_emul_update_irq();
    return 0;
}

void uart_emul_service_host(void)
{
    struct hyper_config *cfg = hyper_config();
    volatile u32 *dr = (volatile u32 *)(cfg->uart.host_base + UARTDR);
    volatile u32 *fr = (volatile u32 *)(cfg->uart.host_base + UARTFR);

    if (!cfg->uart.enabled)
        return;

    while (!(*fr & PL011_UARTFR_RXFE)) {
        if (uart_vcpu_inject_rx((u8)(*dr & 0xff)) != 0)
            break;
    }
}

static int uart_emul_read(struct emul_device *dev, uint64_t addr, int len, uint64_t *value)
{
    u32 off = uart_emul_offset(addr);
    u8 ch;

    switch (off) {
    case UARTDR:
        if (uart_emul_rx_pop(&ch) == 0)
            *value = ch;
        else
            *value = 0;
        uart_emul_update_irq();
        return 0;
    case UARTRSR:
        *value = 0;
        return 0;
    case UARTFR:
        *value = uart_emul_read_fr();
        return 0;
    case UARTIBRD:
        *value = g_uart_emul.ibrd;
        return 0;
    case UARTFBRD:
        *value = g_uart_emul.fbrd;
        return 0;
    case UARTLCR_H:
        *value = g_uart_emul.lcr_h;
        return 0;
    case UARTCR:
        *value = g_uart_emul.cr;
        return 0;
    case UARTIFLS:
        *value = g_uart_emul.ifls;
        return 0;
    case UARTIMSC:
        *value = g_uart_emul.imsc;
        return 0;
    case UARTRIS:
        uart_emul_refresh_rx_irq();
        *value = g_uart_emul.ris;
        return 0;
    case UARTMIS:
        uart_emul_refresh_rx_irq();
        *value = g_uart_emul.ris & g_uart_emul.imsc;
        return 0;
    case UARTDMACR:
        *value = g_uart_emul.dmacr;
        return 0;
    default:
        if (off >= UART_PID4 && off <= UART_CID3) {
            *value = uart_emul_read_id(off);
            return 0;
        }
        *value = 0;
        return 0;
    }
}

static int uart_emul_write(struct emul_device *dev, uint64_t addr, int len, uint64_t value)
{
    u32 off = uart_emul_offset(addr);
    u32 val = value;

    switch (off) {
    case UARTDR:
        uart_emul_putc(val & 0xff);
        return 0;
    case UARTECR:
        return 0;
    case UARTIBRD:
        g_uart_emul.ibrd = val;
        return 0;
    case UARTFBRD:
        g_uart_emul.fbrd = val;
        return 0;
    case UARTLCR_H:
        g_uart_emul.lcr_h = val;
        return 0;
    case UARTCR:
        g_uart_emul.cr = val;
        uart_emul_update_irq();
        return 0;
    case UARTIFLS:
        g_uart_emul.ifls = val;
        return 0;
    case UARTIMSC:
        g_uart_emul.imsc = val & UART_INT_ALL;
        uart_emul_update_irq();
        return 0;
    case UARTICR:
        g_uart_emul.ris &= ~(val & UART_INT_ALL);
        uart_emul_update_irq();
        return 0;
    case UARTDMACR:
        g_uart_emul.dmacr = val;
        return 0;
    default:
        return 0;
    }
}

static struct emul_driver_ops uart_emul_ops = {
    .read = uart_emul_read,
    .write = uart_emul_write,
};

static struct emul_driver uart_emul_driver = {
    .name = "pl011",
    .ops = &uart_emul_ops,
};

void uart_register_emul(void)
{
    memset(&g_uart_emul, 0, sizeof(g_uart_emul));
    g_uart_emul.cr = PL011_UARTCR_UARTEN | PL011_UARTCR_TXE | PL011_UARTCR_RXE;
    g_uart_emul.lcr_h = PL011_UARTLCR_H_WLEN_8 | PL011_UARTLCR_H_FEN;
    register_emul_driver(&uart_emul_driver);
}
