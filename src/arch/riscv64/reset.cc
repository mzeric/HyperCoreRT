#include "safe_printf.h"
#include "utils.h"
#include "inline_asm.h"
#include "exception.h"
#include "mm.h"
#include "vmio.h"
#include "timer.h"
#include "sched.h"
#include "riscv64_system.h"
#include "emul_dev.h"
#include "plic.h"
#include "sbi_helper.h"
#include "smp.h"
#include "percpu.h"

int init_hyper_low_level(void *args) {
    safe_printf("HyperCoreRT RISC-V booting...\n");

    zero_bss();
    setup_exception((void *)__riscv_vector);

    u64 sstatus_val = csrr(sstatus);
    safe_printf("sstatus: 0x%lx\n", sstatus_val);

    /* Allow guest to read time/cycle/instret counters */
    csrw(CSR_SCOUNTEREN, 0x7);
    csrw(CSR_HCOUNTEREN, 0x7);

    init_mm();
    init_emul_dev();
    init_percpu_area();
    plic_init();
    init_sbi();
    smp_boot_secondaries();
    init_sched();
    init_timer();

    /* Enable S-mode external and software interrupts */
    csrs(sie, (1UL << IRQ_S_EXT) | (1UL << IRQ_S_SOFT));

    safe_printf("creating guest task at 0x90080000\n");
    create_task("guest", (void *)0x90080000, 10);

    /*
     * Wait for first timer tick — the timer IRQ triggers sched_yield()
     * which switches into the guest vCPU via VS-mode sret.
     */
    safe_printf("entering scheduler\n");
    while (1) {
        __asm__ volatile("wfi" ::: "memory");
    }

    return 0;
}
