#include "timer.h"
#include "sbi_helper.h"
#include "inline_asm.h"


u64 get_cycles(void)
{
	u64 n;

	__asm__ __volatile__ (
		"rdtime %0"
		: "=r" (n));
	return n;
}

int init_timer(void) {

}

void enable_timer_irq(void) { csrs(sie, (1u) << 5); }

void clear_timer_irq(void) { csrc(sie, (1u << 5)); }

void set_timer_val(u64 val) {
    sbi_set_timer(val);
}

#define QEMU_RISCV_VIRT_FREQ (10000000)
#define HZ (1)

int handle_timer_irq(void) {
    clear_timer_irq();

    set_timer_val(get_cycles() + QEMU_RISCV_VIRT_FREQ/HZ);
    enable_timer_irq();

}