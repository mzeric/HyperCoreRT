#include <inttypes.h>
#include <stdlib.h>
#include "vmmio.h"

extern int init_hyper_low_level(void*);

const char* magic = "HyperCoreRT";

struct test_init_fini{


};

int c_main(void) {
#ifdef CONFIG_BOARD_FVP_AEMVA
    *(volatile int*)0x1c090000 = 'X';
#elif CONFIG_BOARD_QEMU_VIRT
    *(volatile int*)0x09000000 = 'X';
#endif

    init_hyper_low_level(NULL);


    vmm_printf("HyperCoreRT finished\n");
    vmm_printf("----- %s booting -------\n", magic);
    while (1);
        // asm volatile("wfi" ::: "memory");
}

#if defined(__aarch64__)

#elif defined(__x86_64__)
#warning "x86_64"
#elif defined(__riscv)
#warning "riscv"
#else
#warning "unknown arch"
#endif

void _reset(uint64_t arg) {
    safe_printf("stack %p\n", arg);

    c_main();
}
