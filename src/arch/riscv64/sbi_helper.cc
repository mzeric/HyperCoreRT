
#include "htypes.h"
#include "sbi_helper.h"
#include "riscv_sbi.h"

#include "vmio.h"

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5)
{
	struct sbiret ret;

	register uint64_t a0 asm ("a0") = (uint64_t)(arg0);
	register uint64_t a1 asm ("a1") = (uint64_t)(arg1);
	register uint64_t a2 asm ("a2") = (uint64_t)(arg2);
	register uint64_t a3 asm ("a3") = (uint64_t)(arg3);
	register uint64_t a4 asm ("a4") = (uint64_t)(arg4);
	register uint64_t a5 asm ("a5") = (uint64_t)(arg5);
	register uint64_t a6 asm ("a6") = (uint64_t)(fid);
	register uint64_t a7 asm ("a7") = (uint64_t)(ext);
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	ret.error = a0;
	ret.value = a1;

	return ret;
}

void sbi_console_putchar(int ch)
{
	sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

int sbi_console_getchar(void)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_0_1_CONSOLE_GETCHAR, 0, 0, 0, 0, 0, 0, 0);

	return ret.error;
}

int sbi_shutdown(void)
{
	sbi_ecall(SBI_EXT_0_1_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0);

	return 0;
}

void sbi_clear_ipi(void)
{
	sbi_ecall(SBI_EXT_0_1_CLEAR_IPI, 0, 0, 0, 0, 0, 0, 0);
}

static int __sbi_send_ipi_v01(const unsigned long *hart_mask)
{
	sbi_ecall(SBI_EXT_0_1_SEND_IPI, 0,
		  (unsigned long)hart_mask, 0, 0, 0, 0, 0);
	return 0;
}

static void __sbi_set_timer_v01(u64 stime_value)
{
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value, 0, 0, 0, 0, 0);

}

static int __sbi_rfence_v01(unsigned long fid,
			    const unsigned long *hart_mask,
			    unsigned long start, unsigned long size,
			    unsigned long arg4, unsigned long arg5)
{
	int result = 0;

	switch (fid) {
	case SBI_EXT_RFENCE_REMOTE_FENCE_I:
		sbi_ecall(SBI_EXT_0_1_REMOTE_FENCE_I, 0,
			  (unsigned long)hart_mask, 0, 0, 0, 0, 0);
		break;
	case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA:
		sbi_ecall(SBI_EXT_0_1_REMOTE_SFENCE_VMA, 0,
			  (unsigned long)hart_mask, start, size,
			  0, 0, 0);
		break;
	case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID:
		sbi_ecall(SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID, 0,
			  (unsigned long)hart_mask, start, size,
			  arg4, 0, 0);
		break;
	default:
		safe_printf("%s: unknown function ID [%lu]\n", __func__, fid);
		result = -1;
		break;
	};

	return result;
}

static void __sbi_set_timer_v02(u64 stime_value)
{
	sbi_ecall(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, stime_value, 0,
		  0, 0, 0, 0);
}

static int __sbi_send_ipi_v02_real(unsigned long hmask, unsigned long hbase)
{
	struct sbiret ret = {0};
	int result;

	ret = sbi_ecall(SBI_EXT_IPI, SBI_EXT_IPI_SEND_IPI,
			hmask, hbase, 0, 0, 0, 0);
	if (ret.error) {
        return ret.error;
    #if 0
		result = sbi_err_map_errno(ret.error);
		hyper_printf("%s: hmask=0x%lx hbase=%lu failed "
			   "(error %d)\n", __func__, hmask,
			   hbase, result);
    #endif
		return result;
	}

	return 0;
}
static void (*__sbi_set_timer)(u64 stime) = __sbi_set_timer_v01;
static int (*__sbi_send_ipi)(const unsigned long *hart_mask) =
						__sbi_send_ipi_v01;
static int (*__sbi_rfence)(unsigned long fid,
		const unsigned long *hart_mask,
		unsigned long start, unsigned long size,
		unsigned long arg4, unsigned long arg5) = __sbi_rfence_v01;

void sbi_send_ipi(const unsigned long *hart_mask)
{
	__sbi_send_ipi(hart_mask);
}

void sbi_set_timer(u64 stime_value)
{
	__sbi_set_timer(stime_value);
}

void sbi_remote_fence_i(const unsigned long *hart_mask)
{
	__sbi_rfence(SBI_EXT_RFENCE_REMOTE_FENCE_I,
		     hart_mask, 0, 0, 0, 0);
}

void sbi_remote_sfence_vma(const unsigned long *hart_mask,
			   unsigned long start,
			   unsigned long size)
{
	__sbi_rfence(SBI_EXT_RFENCE_REMOTE_SFENCE_VMA,
		     hart_mask, start, size, 0, 0);
}

void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
				unsigned long start,
				unsigned long size,
				unsigned long asid)
{
	__sbi_rfence(SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID,
		     hart_mask, start, size, asid, 0);
}

void sbi_remote_hfence_gvma(const unsigned long *hart_mask,
			    unsigned long start,
			    unsigned long size)
{
	__sbi_rfence(SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA,
		     hart_mask, start, size, 0, 0);
}

void sbi_remote_hfence_gvma_vmid(const unsigned long *hart_mask,
				 unsigned long start,
				 unsigned long size,
				 unsigned long vmid)
{
	__sbi_rfence(SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA_VMID,
		     hart_mask, start, size, vmid, 0);
}

void sbi_remote_hfence_vvma(const unsigned long *hart_mask,
			    unsigned long start,
			    unsigned long size)
{
	__sbi_rfence(SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA,
		     hart_mask, start, size, 0, 0);
}

void sbi_remote_hfence_vvma_asid(const unsigned long *hart_mask,
				 unsigned long start,
				 unsigned long size,
				 unsigned long asid)
{
	__sbi_rfence(SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA_ASID,
		     hart_mask, start, size, asid, 0);
}

static long sbi_ext_base_func(long fid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, fid, 0, 0, 0, 0, 0, 0);
	if (!ret.error)
		return ret.value;
	else
		return ret.error;
}

static int __sbi_send_ipi_v02(const unsigned long *hart_mask)
{
	unsigned long hart, hmask, hbase;
	int result;

	if (!hart_mask) {
		return __sbi_send_ipi_v02_real(0UL, -1UL);
	}

	hmask = hbase = 0;
	// for_each_set_bit(hart, hart_mask, CONFIG_CPU_COUNT) {
    for(hart = 0; hart < 64; hart++) {
        if (!(1u << hart & *hart_mask))
            continue;
		if (hmask && ((hbase + BITS_PER_LONG) <= hart)) {
			result = __sbi_send_ipi_v02_real(hmask, hbase);
			if (result)
				return result;
			hmask = hbase = 0;
		}
		if (!hmask) {
			hbase = hart;
		}
		hmask |= 1UL << (hart - hbase);
	}
	if (hmask) {
		result = __sbi_send_ipi_v02_real(hmask, hbase);
		if (result)
			return result;
	}

	return 0;
}


#define sbi_get_spec_version()		\
	sbi_ext_base_func(SBI_EXT_BASE_GET_SPEC_VERSION)

#define sbi_get_firmware_id()		\
	sbi_ext_base_func(SBI_EXT_BASE_GET_IMP_ID)

#define sbi_get_firmware_version()	\
	sbi_ext_base_func(SBI_EXT_BASE_GET_IMP_VERSION)

int sbi_probe_extension(long extid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, extid,
			0, 0, 0, 0, 0);
	if (!ret.error && ret.value)
		return ret.value;

	return -1;
}
int init_sbi(void) {
    int ver = sbi_get_spec_version();
    safe_printf("sbi version: 0x%x\n", ver);
    safe_printf("sbi fw id:%x, version:%x\n", sbi_get_firmware_id(), sbi_get_firmware_version());
    safe_printf("time_ext: %d, pip_ext:%d, rfence_ext:%d, srst_ext:%d\n",
                sbi_probe_extension(SBI_EXT_TIME),
                sbi_probe_extension(SBI_EXT_IPI),
                sbi_probe_extension(SBI_EXT_RFENCE),
                sbi_probe_extension(SBI_EXT_SRST));
}