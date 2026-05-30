#include <inttypes.h>
#include <stdlib.h>
#include "vmio.h"

int init_hyper_low_level(void*);
int modern_cpp_smoke(void);
int modern_cpp_global_ctor_smoke(void);
int modern_cpp_raii_lock_smoke(void);
void cxx_run_global_ctors(void);

const char* magic = "HyperCoreRT";

struct test_init_fini{


};

int c_main(uintptr_t dtb_phys) {

    cxx_run_global_ctors();
    safe_printf("modern cpp smoke: %d\n", modern_cpp_smoke());
    safe_printf("modern cpp global ctor: %d\n", modern_cpp_global_ctor_smoke());
    safe_printf("modern cpp raii lock: %d\n", modern_cpp_raii_lock_smoke());
    init_hyper_low_level((void *)dtb_phys);
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

extern "C" void _reset(uint64_t dtb_phys) {
    c_main((uintptr_t)dtb_phys);
}
