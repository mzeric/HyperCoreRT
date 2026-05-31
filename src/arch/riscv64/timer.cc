#include "timer.h"
#include "sbi_helper.h"
#include "inline_asm.h"
#include "safe_printf.h"

#define QEMU_RISCV_VIRT_FREQ (10000000)
#define TIMER_TICK_HZ        (100) /* 10ms tick */

u64 get_cycles(void) {
    u64 n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
}

void enable_timer_irq(void) { csrs(sie, 1UL << 5); }

void clear_timer_irq(void) { csrc(sie, 1UL << 5); }

void set_timer_val(u64 val) { sbi_set_timer(val); }

void hyp_timer_rearm(void) {
    u64 next = get_cycles() + QEMU_RISCV_VIRT_FREQ / TIMER_TICK_HZ;
    sbi_set_timer(next);
    enable_timer_irq();
}

int init_timer(void) {
    hyp_timer_rearm();
    return 0;
}

int handle_timer_irq(void) {
    clear_timer_irq();
    hyp_timer_rearm();
    return 0;
}
