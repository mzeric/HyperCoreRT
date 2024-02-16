#include <sys/stat.h>
#include <stdio.h>

// #if defined(__aarch64__)
enum {
    UART_FR_RXFE = 0x10,
    UART_FR_TXFF = 0x0,
    UART0_ADDR = 0x09000000,
};
// #endif

#define UART_DR(baseaddr) (*(unsigned int*)(baseaddr))
#define UART_FR(baseaddr) (*(((unsigned int*)(baseaddr)) + 6))

int _close(int file) { return -1; }

int _fstat(int file, struct stat* st) {
    // st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) { return 1; }

int _lseek(int file, int ptr, int dir) { return 0; }

int _open(const char* name, int flags, int mode) { return -1; }

int _read(int file, char* ptr, int len) {
    int todo;

    // FIXME:
    return 0;

    if (len == 0) {
        return 0;
    }

    while (UART_FR(UART0_ADDR) & UART_FR_RXFE)
        ;

    *ptr++ = UART_DR(UART0_ADDR);

    for (todo = 1; todo < len; todo++) {
        if (UART_FR(UART0_ADDR) & UART_FR_RXFE) {
            break;
        }

        *ptr++ = UART_DR(UART0_ADDR);
    }

    return todo;
}

char* heap_end = NULL;
caddr_t _sbrk(int incr) {
    extern char _heap_low_; /* Defined by the linker */
    extern char _heap_top_; /* Defined by the linker */
    char* prev_heap_end;


    if (heap_end == 0) {
        heap_end = &_heap_low_;
    }

    prev_heap_end = heap_end;

    if (heap_end + incr > &_heap_top_) {
        /* Heap and stack collision */
        return (caddr_t)0;
    }

    heap_end += incr;

    return (caddr_t)prev_heap_end;
}

int _write(int file, char* ptr, int len) {
    int todo;

    for (todo = 0; todo < len; todo++) {
        UART_DR(UART0_ADDR) = *ptr++;
    }

    return len;
}

void _exit(int status) {
    printf("exit....\n");
    while (1)
        ;
}

int _kill(int pid, int sig) { return 0; }

int _getpid(int pid) { return 0; }
