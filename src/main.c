#include <inttypes.h>
#include <stdlib.h>
#include "vmio.h"

int init_hyper_low_level(void*);
int modern_cpp_smoke(void);
int modern_cpp_global_ctor_smoke(void);
void cxx_run_global_ctors(void);

const char* magic = "HyperCoreRT";

struct test_init_fini{


};

int c_main(void) {

    cxx_run_global_ctors();
    safe_printf("modern cpp smoke: %d\n", modern_cpp_smoke());
    safe_printf("modern cpp global ctor: %d\n", modern_cpp_global_ctor_smoke());
    init_hyper_low_level(NULL);
    while(1);


    hyper_printf("HyperCoreRT finished\n");
    hyper_printf("----- %s booting -------\n", magic);
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
