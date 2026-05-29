#pragma once
#include "arch_barrier.h"
#include "inline_asm.h"
#include "htypes.h"
#include "vcpu.h"
#include "vmio.h"

#include <inttypes.h>

#define GENERIC_TIMER_CTRL_ENABLE		(1 << 0)
#define GENERIC_TIMER_CTRL_IT_MASK		(1 << 1)
#define GENERIC_TIMER_CTRL_IT_STAT		(1 << 2)

enum {
	GENERIC_TIMER_REG_FREQ,
	GENERIC_TIMER_REG_HCTL,
	GENERIC_TIMER_REG_KCTL,
	GENERIC_TIMER_REG_HYP_CTRL,
	GENERIC_TIMER_REG_HYP_TVAL,
	GENERIC_TIMER_REG_HYP_CVAL,
	GENERIC_TIMER_REG_PHYS_CTRL,
	GENERIC_TIMER_REG_PHYS_TVAL,
	GENERIC_TIMER_REG_PHYS_CVAL,
	GENERIC_TIMER_REG_VIRT_CTRL,
	GENERIC_TIMER_REG_VIRT_TVAL,
	GENERIC_TIMER_REG_VIRT_CVAL,
	GENERIC_TIMER_REG_VIRT_OFF,
};

static inline u64 generic_timer_reg_read(int reg)
{
	u64 val;

	switch (reg) {
	case GENERIC_TIMER_REG_FREQ:
		val = mrs(cntfrq_el0);
		break;
	case GENERIC_TIMER_REG_HCTL:
		val = mrs(cnthctl_el2);
		break;
	case GENERIC_TIMER_REG_KCTL:
		val = mrs(cntkctl_el1);
		break;
	case GENERIC_TIMER_REG_HYP_CTRL:
		val = mrs(cnthp_ctl_el2);
		break;
	case GENERIC_TIMER_REG_HYP_TVAL:
		val = mrs(cnthp_tval_el2);
		break;
	case GENERIC_TIMER_REG_PHYS_CTRL:
		val = mrs(cntp_ctl_el0);
		break;
	case GENERIC_TIMER_REG_PHYS_TVAL:
		val = mrs(cntp_tval_el0);
		break;
	case GENERIC_TIMER_REG_VIRT_CTRL:
		val = mrs(cntv_ctl_el0);
		break;
	case GENERIC_TIMER_REG_VIRT_TVAL:
		val = mrs(cntv_tval_el0);
		break;
	default:
		hyper_fatal("Trying to read invalid generic-timer register\n");
	}

	return val;
}

static inline void generic_timer_reg_write(int reg, u32 val)
{
	switch (reg) {
	case GENERIC_TIMER_REG_FREQ:
		msr(cntfrq_el0, val);
		break;
	case GENERIC_TIMER_REG_HCTL:
		msr(cnthctl_el2, val);
		break;
	case GENERIC_TIMER_REG_KCTL:
		msr(cntkctl_el1, val);
		break;
	case GENERIC_TIMER_REG_HYP_CTRL:
		msr(cnthp_ctl_el2, val);
		break;
	case GENERIC_TIMER_REG_HYP_TVAL:
		msr(cnthp_tval_el2, val);
		break;
	case GENERIC_TIMER_REG_PHYS_CTRL:
		msr(cntp_ctl_el0, val);
		break;
	case GENERIC_TIMER_REG_PHYS_TVAL:
		msr(cntp_tval_el0, val);
		break;
	case GENERIC_TIMER_REG_VIRT_CTRL:
		msr(cntv_ctl_el0, val);
		break;
	case GENERIC_TIMER_REG_VIRT_TVAL:
		msr(cntv_tval_el0, val);
		break;
	default:
		hyper_fatal("Trying to write invalid generic-timer register\n");
	}

	isb();
}

static u32 __attribute__((unused)) timer_vcpu_irq_handler(int irq)
{
	u32 ctl;
	ctl = generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_CTRL);
	if (!(ctl & GENERIC_TIMER_CTRL_IT_STAT)) {
		/* We got interrupt without status bit set.
		 * Looks like we are running on buggy hardware.
		 */
		hyper_printf("%s: suprious interrupt\n", __func__);
        return 0;
	}

	ctl |= GENERIC_TIMER_CTRL_IT_MASK;
	generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, ctl);

	return 1;
}