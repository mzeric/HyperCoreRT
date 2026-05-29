#pragma once
#include "htypes.h"
#include "arch_regs.h"
struct cpu_vcpu_trap {
	unsigned long sepc;
	unsigned long scause;
	unsigned long stval;
	unsigned long htval;
	unsigned long htinst;
};

/* MRS/MSR Trap ISS Encodings */
#define ISS_SYSREG_ENC(op0, op2, op1, crn, crm)		(((op0) << 20) | \
							 ((op2) << 17) | \
							 ((op1) << 14)  | \
							 ((crn) << 10) | \
							 ((crm) << 1))

int inject_illegal_inst(struct cpu_user_regs *regs, uint64_t inst);