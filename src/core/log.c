#include "log.h"
#include "safe_printf.h"
#include <stdint.h>

/* Global runtime log level — default matches compile-time threshold. */
int g_log_level = LOG_LEVEL;

/* Output mode: default dual output (UART + ring buffer). */
static unsigned int g_log_output = LOG_OUTPUT_BOTH;

/* Ring buffer instance. */
static struct {
    char     data[LOG_BUF_SIZE];
    uint32_t head;      /* next write position */
    uint32_t tail;      /* next read position  */
    uint32_t overflow;  /* number of dropped characters */
} log_buf;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static void buf_write(char ch)
{
    uint32_t next = (log_buf.head + 1) % LOG_BUF_SIZE;
    if (next == log_buf.tail) {
        /* Buffer full — drop the oldest character. */
        log_buf.tail = (log_buf.tail + 1) % LOG_BUF_SIZE;
        log_buf.overflow++;
    }
    log_buf.data[log_buf.head] = ch;
    log_buf.head = next;
}

static char buf_read(void)
{
    if (log_buf.head == log_buf.tail)
        return 0;  /* empty */
    char ch = log_buf.data[log_buf.tail];
    log_buf.tail = (log_buf.tail + 1) % LOG_BUF_SIZE;
    return ch;
}

static int buf_empty(void)
{
    return log_buf.head == log_buf.tail;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void log_putchar(char ch)
{
    if (g_log_output & LOG_OUTPUT_BUF)
        buf_write(ch);
    if (g_log_output & LOG_OUTPUT_UART)
        arch_putchar(ch);
}

void log_flush(void)
{
    if (log_buf.overflow) {
        safe_printf("\n--- %lu log chars dropped ---\n",
                    (unsigned long)log_buf.overflow);
    }
    while (!buf_empty())
        arch_putchar(buf_read());
}

void log_set_level(int level)
{
    g_log_level = level;
}

int log_get_level(void)
{
    return g_log_level;
}

void log_set_output(unsigned int mode)
{
    g_log_output = mode;
}
