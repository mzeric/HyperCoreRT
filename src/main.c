#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

const char* magic = "HyperCoreRT\n";

void out_c(char c) { *(volatile uint32_t*)0x09000000 = c; }

void out_str(const char* str) {
    while (*str != 0) {
        out_c(*str);
        str++;
    }
}

char itoh(int i) {
    if (i >= 0 && i <= 9)
        return '0' + i;
    if (i >= 0xA && i <= 0xF)
        return 'A' + i - 10;
    return '@';
}

void _itoa(uint32_t data, char* buf) {
    int i = 7;
    while (i >= 0) {
        buf[7 - i] = itoh(data >> (i * 4) & 0xf);
        i--;
    }
}

struct test_init_fini{


};

void out_hex(uint32_t data) {
    char buf[9];
    _itoa(data, buf);
    buf[8] = 0;
    out_str(buf);
}

int c_main(void) {
    out_c('^');
    printf("----- booting -------\n");
    // while(1);
    void* addr = malloc(4);
    printf("malloc:%p\n", addr);
    char* hello = "HELLO";
    out_str(magic);

vmm_devemu_emulate_read();
    while (1)
        ;
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
