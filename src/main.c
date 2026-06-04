#include <inttypes.h>
#include <stdlib.h>
#include "vmio.h"

int init_hyper_low_level(void*);
#if defined(__riscv)
void riscv_set_boot_hart_id(int hart_id);
#endif

const char* magic = "HyperCoreRT";

int c_main(uintptr_t dtb_phys) {

    init_hyper_low_level((void *)dtb_phys);
    while(1);


    hyper_printf("HyperCoreRT finished\n");
    hyper_printf("----- %s booting -------\n", magic);
    while (1);
        // asm volatile("wfi" ::: "memory");
}

void _reset(uint64_t arg0, uint64_t arg1) {
#if defined(__riscv)
    riscv_set_boot_hart_id((int)arg0);
    c_main((uintptr_t)arg1);
#else
    c_main((uintptr_t)arg0);
#endif
}
