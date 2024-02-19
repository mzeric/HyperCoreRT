#include <inttypes.h>
#include <stdlib.h>
#include "vmmio.h"

extern int init_hyper_low_level(void*);

const char* magic = "HyperCoreRT";

struct test_init_fini{


};

int c_main(void) {
    vmm_printf("----- %s booting -------\n", magic);

    init_hyper_low_level(NULL);

    vmm_printf("HyperCoreRT finished\n");
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

void _reset(void) { c_main(); }
