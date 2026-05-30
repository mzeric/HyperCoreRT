#pragma once

#include "vcpu.h"
#include "sched.h"

/* Sysreg access */
#define gic_read_sysreg(__r) ({					\
	u64 __v;						\
	asm volatile("mrs %0, " stringify(__r) : "=r" (__v));	\
	__v;							\
})

#define gic_write_sysreg(__v, __r)	do {				\
	asm volatile("msr " stringify(__r) ", %0"		\
		     : : "r" ((u64)(__v)));			\
} while (0)

#define arch_gic_read_sysreg(__sysreg)		gic_read_sysreg(__sysreg)
#define arch_gic_write_sysreg(__val, __sysreg)	gic_write_sysreg(__val, __sysreg)

#define __LR0_EL2(x)              S3_4_C12_C12_ ## x
#define __LR8_EL2(x)              S3_4_C12_C13_ ## x

#define ICH_LR0_EL2               __LR0_EL2(0)
#define ICH_LR1_EL2               __LR0_EL2(1)
#define ICH_LR2_EL2               __LR0_EL2(2)
#define ICH_LR3_EL2               __LR0_EL2(3)
#define ICH_LR4_EL2               __LR0_EL2(4)
#define ICH_LR5_EL2               __LR0_EL2(5)
#define ICH_LR6_EL2               __LR0_EL2(6)
#define ICH_LR7_EL2               __LR0_EL2(7)
#define ICH_LR8_EL2               __LR8_EL2(0)
#define ICH_LR9_EL2               __LR8_EL2(1)
#define ICH_LR10_EL2              __LR8_EL2(2)
#define ICH_LR11_EL2              __LR8_EL2(3)
#define ICH_LR12_EL2              __LR8_EL2(4)
#define ICH_LR13_EL2              __LR8_EL2(5)
#define ICH_LR14_EL2              __LR8_EL2(6)
#define ICH_LR15_EL2              __LR8_EL2(7)

static inline void arch_gic_write_eoir(u32 irq)
{
	asm volatile("msr_s " stringify(ICC_EOIR1_EL1) ", %0"
			: : "r" ((u64)irq));
	isb();
}

static inline void arch_gic_write_dir(u32 irq)
{
	asm volatile("msr_s " stringify(ICC_DIR_EL1) ", %0"
			: : "r" ((u64)irq));
	isb();
}

struct gic_vcpu_sgi {
	u8 intid;
	u8 aff1;
	u8 aff2;
	u8 aff3;
	u8 rs;
	u32 target_list;
	bool irm;
};

int gic_vcpu_send_sgi(const struct gic_vcpu_sgi *sgi);

/* Queue a vIRQ (PPI or SGI) for delivery to the given task's vcpu.
   The interrupt is flushed to ICH_LR on the next context restore. */
void gic_vcpu_inject_virq(hyper_task_t *task, int virq);

/* Find a task by its dts-reg/MPIDR.aff value. NULL if not found. */
hyper_task_t *find_task_by_mpidr(uint64_t mpidr);

/* Enable virtual CPU interface (ICH_HCR_EL2 / ICH_VMCR_EL2). Call once after
   physical GIC init, and again after each cold-init on a new pCPU. */
void gic_vcpu_init_pcpu(void);

void gic_vcpu_save(vcpu_t *vcpu);
void gic_vcpu_restore(vcpu_t *vcpu);

/* Called from __el2_switch_to to push pending vIRQs into ICH_LR for the
   next-to-run task. */
void gic_vcpu_flush_lr(hyper_task_t *task);
