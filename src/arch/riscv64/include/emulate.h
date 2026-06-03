#pragma once
#include "htypes.h"
#include "arch_regs.h"
#include "vcpu.h"

struct cpu_vcpu_trap {
	unsigned long sepc;
	unsigned long scause;
	unsigned long stval;
	unsigned long htval;
	unsigned long htinst;
};

int vcpu_emulate_mmio(vcpu_t *vcpu, struct cpu_user_regs *regs,
                      uint64_t fault_addr, int is_write);
int vcpu_redirect_trap(struct cpu_user_regs *regs, struct cpu_vcpu_trap *trap);
int inject_illegal_inst(struct cpu_user_regs *regs, uint64_t inst);
