#include "vmio.h"
#include "io.h"


uint32_t readl(void *addr) { return *(volatile uint32_t *)addr; }

void writel(void *addr, uint32_t value) { *(volatile uint32_t *)addr = value; }

void zero_bss(void) {
    extern int _bss_start, _bss_end;
    size_t size = (size_t)&_bss_end - (size_t)&_bss_start;
    uint64_t *ptr = (uint64_t *)&_bss_start;
    size /= sizeof(*ptr);
    while(size--) {
        *ptr++ = 0;
    }
    // memset(&_bss_start, 0, size);
}
