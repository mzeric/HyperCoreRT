
#include "safe_printf.h"
#include "utils.h"
#include "sbi.h"
#include "inline_asm.h"
#include "exception.h"
#include "mm.h"
#include "vmio.h"
#include "timer.h"
#include "sbi_helper.h"
#include "sched.h"

// static inline void sbi_set_timer(unsigned long stime_value)
// {
// 	SBI_CALL_1(SBI_SET_TIMER, stime_value);
// }

static inline void sbi_putchar(char c)
{
	SBI_CALL_1(SBI_CONSOLE_PUTCHAR, c);
}

static inline void sbi_put_string(char *str)
{
	int i;

	for (i = 0; str[i] != '\0'; i++)
		sbi_putchar((char) str[i]);
}

void trigger_load_access_fault(void);


void switch_to_vs();

volatile int delay_ns(int ns) {
    volatile int m = ns;
        while (m--)
            ;
}

#include "exception.h"


static void hyper_init_entry(void) {

    // csrc(CSR_HSTATUS, (1<<21));
    // setup_exception(&__riscv_vector);

    // safe_printf("init, current_el:%lx\n", csrr(CSR_HSTATUS));
    // while(1);
    // switch_to_vs();
    // switch_to_el1();
    // csrr(hedeleg);

    while (1) {
        // asm("wfi");
        // safe_printf("ss: %lx\n", csrr(sstatus));
        // csrs(sstatus, 0x2u);
        delay_ns(0x10000000);

        // *(volatile int *)0x30000000 = '-';
        safe_printf("init wakeup at el\n");
    }
}

static void hyper_idle(void) {
    safe_printf("idle\n");
    while (1) {
		asm ("wfi");
					// csrs(sstatus, 0x2u);
        safe_printf("idle wakeup at el\n");
    }
}
int init_hyper_low_level(void *args) {
    int mpp;
    int m_mode = 0x3;
	safe_printf("hello,riscv-64\n");

	zero_bss();
	setup_exception(__riscv_vector);

	sbi_putchar('X');


	// csrr(mstatus);
	mpp = csrr(sstatus);
	safe_printf("s-status: 0x%x\n", mpp);
	// safe_printf("deleg: %x\n", v);


	init_mm();
    init_sched();

	init_timer();

	init_sbi();
	int cycle = get_cycles();
	sbi_set_timer(cycle + 0x100);
	safe_printf("hello\n");
	safe_printf("world\n");

#if 1
    // create_task("init", hyper_init_entry, 10);
    create_task("guest", 0x90080000, 10);

    // create_task("idle", hyper_idle, 10);
#endif
	while(1);
#if 0
    asm volatile (
        "csrr %[mpp], mstatus"   "\n\t"
        "andi %[mpp], %[mpp], %[mpp_mask]" "\n\t"
        : [mpp] "=r" (mpp)
        : [mpp_mask] "i" (3)
    );
#endif


	safe_printf("init done\n");

	return 0;
}
