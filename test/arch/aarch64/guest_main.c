#include "safe_printf.h"
#include "htypes.h"
#include "inline_asm.h"

void arch_putchar(char c) {
    *(volatile int*)0x09000000 = c;
}

/* Guest test has no ring buffer — forward directly to UART. */
void log_putchar(char ch) {
    arch_putchar(ch);
}
#undef wfi
#define wfi()				\
	({asm volatile(			\
	"wfi" : : : "memory");		\
	})
#define stringify(x) #x
#undef write_sysreg
#define write_sysreg(__v, __r)                                                 \
    do {                                                                       \
        asm volatile("msr " stringify(__r) ", %0\n\t"                              \
                                           "dsb sy\n\t"                        \
                                           "isb\n\t"                           \
                     :                                                         \
                     : "r"((uint64_t)(__v)));                                  \
    } while (0)
extern void* __hyp_vectors;

void _reset(void) {

    safe_printf("hello,Guest\n");
    write_sysreg(&__hyp_vectors, vbar_el1);

    // msr(vbar_el1, 0);


    int d = *(volatile int*)0x50000000;
    safe_printf("guest read: %c\n", d);

    *(volatile int*)0x50000000 = 'X';

    mrs(actlr_el1);
    // while(1);
    while(1){
        wfi();
        safe_printf("loop,Guest\n");

    }
}