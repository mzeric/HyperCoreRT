#include "pl011.h"
#include "htypes.h"
#include "config.h"

#ifdef CONFIG_BOARD_FVP_AEMVA
#define UART0_ADDR 0x1c090000
#else
#define UART0_ADDR 0x09000000
#endif



int pl011_init(uintptr_t base) {
    (void)base;
    return 0;
}

int  pl011_putc(const char c) { *(volatile int *)UART0_ADDR = c; return 0; }
