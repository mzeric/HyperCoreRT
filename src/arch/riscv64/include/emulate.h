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

int inject_illegal_inst(struct cpu_user_regs *regs, uint64_t inst);