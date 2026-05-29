#include "safe_printf.h"
#include "inline_asm.h"
#include "riscv64_system.h"

void arch_putchar(char c) {
    *(volatile int *)0x20000000 = c;
}

void setup_traps();

void _reset(void) {
    safe_printf("hello,guest\n");
    setup_traps();


    while (1)
        ;
}

