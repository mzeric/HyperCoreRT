#pragma once
#include "htypes.h"
int init_sbi(void);

struct sbiret {
	long error;
	long value;
};

struct sbiret sbi_ecall(int ext, int fid, uint64_t arg0,
			uint64_t arg1, uint64_t arg2,
			uint64_t arg3, uint64_t arg4,
			uint64_t arg5);
void sbi_send_ipi(const unsigned long *hart_mask);

/**
 * Program the timer for next timer event.
 *
 * @param stime_value Timer value after which next timer event should fire.
 */
void sbi_set_timer(u64 stime_value);