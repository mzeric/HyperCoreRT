#pragma once
#include "htypes.h"
#include "arch_regs.h"
#include "vcpu.h"

enum TRAP_REG_SIZE {
    trap_reg_int8 = 0,
    trap_reg_int16 = 1,
    trap_reg_int32 = 2,
    trap_reg_int64 = 3,
};

/* MRS/MSR Trap ISS Encodings */
#define ISS_SYSREG_ENC(op0, op2, op1, crn, crm)		(((op0) << 20) | \
							 ((op2) << 17) | \
							 ((op1) << 14)  | \
							 ((crn) << 10) | \
							 ((crm) << 1))
#define ISS_SYSREG_MASK					0xfffffc1e

#define ISS_CPACR_EL1					ISS_SYSREG_ENC(3,2,0,1,0)
#define ISS_CNTFRQ_EL0					ISS_SYSREG_ENC(3,0,3,14,0)
#define ISS_CNTPCT_EL0					ISS_SYSREG_ENC(3,1,3,14,0)
#define ISS_CNTVCT_EL0					ISS_SYSREG_ENC(3,2,3,14,0)
#define ISS_CNTKCTL_EL1					ISS_SYSREG_ENC(3,0,0,14,1)
#define ISS_CNTP_TVAL_EL0				ISS_SYSREG_ENC(3,0,3,14,2)
#define ISS_CNTP_CTL_EL0				ISS_SYSREG_ENC(3,1,3,14,2)
#define ISS_CNTP_CVAL_EL0				ISS_SYSREG_ENC(3,2,3,14,2)
#define ISS_CNTV_TVAL_EL0				ISS_SYSREG_ENC(3,0,3,14,3)
#define ISS_CNTV_CTL_EL0				ISS_SYSREG_ENC(3,1,3,14,3)
#define ISS_CNTV_CVAL_EL0				ISS_SYSREG_ENC(3,2,3,14,3)
#define ISS_ACTLR_EL1					ISS_SYSREG_ENC(3,1,0,1,0)
#define ISS_ACTLR_EL2					ISS_SYSREG_ENC(3,1,0,1,4)
#define ISS_ACTLR_EL3					ISS_SYSREG_ENC(3,1,0,1,6)
#define ISS_DCISW_EL1					ISS_SYSREG_ENC(1,2,0,7,6)
#define ISS_DCCSW_EL1					ISS_SYSREG_ENC(1,2,0,7,10)
#define ISS_DCCISW_EL1					ISS_SYSREG_ENC(1,2,0,7,14)


// GIC v3
#define ISS_SRE_EL1			ISS_SYSREG_ENC(3, 5, 0, 12, 12)



uint64_t vcpu_reg_read(struct cpu_user_regs *regs, int id, int size);
uint64_t vcpu_reg_write(struct cpu_user_regs *regs, int id, int size, uint64_t value);

int vcpu_emulate_read(vcpu_t *vcpu,struct cpu_user_regs *regs, paddr_t ipa, int reg_id, int size);
int vcpu_emulate_write(vcpu_t *vcpu, struct cpu_user_regs *regs, paddr_t ipa, int reg_id, int size);
int vcpu_emulate_sysreg_read(struct cpu_user_regs *regs, uint64_t iss, uint64_t *data);
int vcpu_emulate_sysreg_write(struct cpu_user_regs *regs, uint64_t iss, uint64_t data);
