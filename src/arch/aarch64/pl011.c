#include "pl011.h"
#include "autoconf.h"

#ifdef CONFIG_BOARD_FVP_AEMVA
#define UART0_ADDR 0x1c090000
#else
#define UART0_ADDR 0x09000000
#endif

void pl011_putc(char c) { *(volatile int *)UART0_ADDR = c; }