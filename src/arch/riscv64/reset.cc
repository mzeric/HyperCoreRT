#include "safe_printf.h"
#include "utils.h"
#include "inline_asm.h"
#include "exception.h"
#include "mm.h"
#include "vmio.h"
#include "timer.h"
#include "sched.h"
#include "init.h"

int init_hyper_low_level(void *args) {
    safe_printf("HyperCoreRT RISC-V booting...\n");

    zero_bss();
    setup_exception((void *)__riscv_vector);

    u64 sstatus_val = csrr(sstatus);
    safe_printf("sstatus: 0x%lx\n", sstatus_val);

    init_mm();
    init_sched();
    init_timer();

    safe_printf("creating guest task at 0x90080000\n");
    create_task("guest", (void *)0x90080000, 10);

    safe_printf("entering scheduler loop\n");
    while (1) {
        __asm__ volatile("wfi" ::: "memory");
    }

    return 0;
}
