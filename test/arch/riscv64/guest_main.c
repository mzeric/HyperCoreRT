#include "safe_printf.h"
#include "inline_asm.h"
#include "riscv64_system.h"
#include "riscv_sbi.h"

struct guest_sbiret {
    long error;
    long value;
};

void arch_putchar(char c) {
    *(volatile int *)0x20000000 = c;
}

void log_putchar(char ch) {
    arch_putchar(ch);
}

static struct guest_sbiret guest_sbi_ecall(unsigned long ext, unsigned long fid,
                                           unsigned long arg0, unsigned long arg1,
                                           unsigned long arg2, unsigned long arg3,
                                           unsigned long arg4, unsigned long arg5) {
    register unsigned long a0 asm("a0") = arg0;
    register unsigned long a1 asm("a1") = arg1;
    register unsigned long a2 asm("a2") = arg2;
    register unsigned long a3 asm("a3") = arg3;
    register unsigned long a4 asm("a4") = arg4;
    register unsigned long a5 asm("a5") = arg5;
    register unsigned long a6 asm("a6") = fid;
    register unsigned long a7 asm("a7") = ext;

    asm volatile("ecall"
                 : "+r"(a0), "+r"(a1)
                 : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                 : "memory");

    struct guest_sbiret ret = {(long)a0, (long)a1};
    return ret;
}

static long guest_sbi_probe(unsigned long ext) {
    struct guest_sbiret ret = guest_sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT,
                                              ext, 0, 0, 0, 0, 0);
    if (ret.error != SBI_SUCCESS)
        return 0;
    return ret.value ? 1 : 0;
}

static void run_sbi_diag(void) {
    long base = guest_sbi_probe(SBI_EXT_BASE);
    long time = guest_sbi_probe(SBI_EXT_TIME);
    long ipi = guest_sbi_probe(SBI_EXT_IPI);
    long rfence = guest_sbi_probe(SBI_EXT_RFENCE);
    long hsm = guest_sbi_probe(SBI_EXT_HSM);

    safe_printf("[sbi_diag] base=%ld time=%ld ipi=%ld rfence=%ld hsm=%ld\n",
                base, time, ipi, rfence, hsm);

    struct guest_sbiret ret = guest_sbi_ecall(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER,
                                              ~0UL, 0, 0, 0, 0, 0);
    if (ret.error == SBI_SUCCESS && ret.value == 0)
        safe_printf("[sbi_diag] time_set_timer=ok\n");
    else
        safe_printf("[sbi_diag] time_set_timer=error:%ld value:%ld\n", ret.error, ret.value);

    ret = guest_sbi_ecall(SBI_EXT_RFENCE, 0xdeadUL, 0, 0, 0, 0, 0, 0);
    if (ret.error == SBI_ERR_NOT_SUPPORTED && ret.value == 0)
        safe_printf("[sbi_diag] rfence_bad_fid=ok\n");
    else
        safe_printf("[sbi_diag] rfence_bad_fid=error:%ld value:%ld\n", ret.error, ret.value);
}

void _reset(void) {
    safe_printf("hello,guest\n");
    run_sbi_diag();

    while (1)
        ;
}

