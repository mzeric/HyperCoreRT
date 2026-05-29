#pragma once
#include "arch_regs.h"
#include "vmio.h"
#include "vcpu.h"
#include "arch_barrier.h"
#include "sched.h"
#include "hyper_config.h"
#include "emulate.h"

/* PSCI v0.2 interface */
#define PSCI_0_2_FN_BASE			0x84000000
#define PSCI_0_2_FN(n)				(PSCI_0_2_FN_BASE + (n))
#define PSCI_0_2_64BIT				0x40000000
#define PSCI_0_2_FN64_BASE			\
					(PSCI_0_2_FN_BASE + PSCI_0_2_64BIT)
#define PSCI_0_2_FN64(n)			(PSCI_0_2_FN64_BASE + (n))

#define PSCI_0_2_FN_PSCI_VERSION		PSCI_0_2_FN(0)
#define PSCI_0_2_FN_CPU_SUSPEND			PSCI_0_2_FN(1)
#define PSCI_0_2_FN_CPU_OFF			PSCI_0_2_FN(2)
#define PSCI_0_2_FN_CPU_ON			PSCI_0_2_FN(3)
#define PSCI_0_2_FN_AFFINITY_INFO		PSCI_0_2_FN(4)
#define PSCI_0_2_FN_MIGRATE			PSCI_0_2_FN(5)
#define PSCI_0_2_FN_MIGRATE_INFO_TYPE		PSCI_0_2_FN(6)
#define PSCI_0_2_FN_MIGRATE_INFO_UP_CPU		PSCI_0_2_FN(7)
#define PSCI_0_2_FN_SYSTEM_OFF			PSCI_0_2_FN(8)
#define PSCI_0_2_FN_SYSTEM_RESET		PSCI_0_2_FN(9)

#define PSCI_0_2_FN64_CPU_SUSPEND		PSCI_0_2_FN64(1)
#define PSCI_0_2_FN64_CPU_ON			PSCI_0_2_FN64(3)
#define PSCI_0_2_FN64_AFFINITY_INFO		PSCI_0_2_FN64(4)
#define PSCI_0_2_FN64_MIGRATE			PSCI_0_2_FN64(5)
#define PSCI_0_2_FN64_MIGRATE_INFO_UP_CPU	PSCI_0_2_FN64(7)

/* PSCI v0.2 multicore support in Trusted OS returned by MIGRATE_INFO_TYPE */
#define PSCI_0_2_TOS_UP_MIGRATE			0
#define PSCI_0_2_TOS_UP_NO_MIGRATE		1
#define PSCI_0_2_TOS_MP				2

#define PSCI_RET_SUCCESS			0
#define PSCI_RET_INVALID_PARAMETERS		(-2)
#define PSCI_RET_DENIED				(-3)
#define PSCI_RET_ALREADY_ON			(-4)

static u64 psci_mpidr_affinity(u64 mpidr)
{
	return mpidr & 0xff00ffffffULL;
}

static bool psci_configured_mpidr(u64 cpu_id, u64 *mpidr)
{
	struct hyper_config *cfg = hyper_config();
	u64 target = psci_mpidr_affinity(cpu_id);

	for (u32 i = 0; i < cfg->guest.vcpu_count; ++i) {
		u64 configured = psci_mpidr_affinity(cfg->guest.vcpu_mpidr[i]);
		if (configured == target) {
			if (mpidr)
				*mpidr = configured;
			return true;
		}
	}
	return false;
}

static bool psci_task_exists(u64 mpidr)
{
	extern hyper_task_t *g_current_task;
	extern struct list_head g_ready_list;
	hyper_task_t *iter;

	mpidr = psci_mpidr_affinity(mpidr);
	if (g_current_task && psci_mpidr_affinity(g_current_task->mpidr) == mpidr)
		return true;
	list_for_each_entry(iter, &g_ready_list, list) {
		if (psci_mpidr_affinity(iter->mpidr) == mpidr)
			return true;
	}
	return false;
}

static unsigned long psci_vcpu_on(/*vcpu_t *source_vcpu, */struct cpu_user_regs *regs)
{
	unsigned long cpu_id;
	unsigned long context_id;
	unsigned long target_pc;
	u64 target_mpidr;

	cpu_id     = vcpu_reg_read(regs, 1, 0);
	target_pc  = vcpu_reg_read(regs, 2, 0);
	context_id = vcpu_reg_read(regs, 3, 0);

	hyper_info("psci_vcpu_on: target_cpu=0x%lx pc=0x%lx ctx=0x%lx\n",
	         cpu_id, target_pc, context_id);

	if (!psci_configured_mpidr(cpu_id, &target_mpidr)) {
		hyper_warn("psci_vcpu_on: reject unconfigured mpidr 0x%lx\n", cpu_id);
		return PSCI_RET_INVALID_PARAMETERS;
	}
	if (psci_task_exists(target_mpidr))
		return PSCI_RET_ALREADY_ON;

	arch_smp_mb();

	if (create_task2("smp", (void *)target_pc, 10))
		return PSCI_RET_DENIED;

	extern struct list_head g_ready_list;
	hyper_task_t *new_task = NULL, *iter;
	list_for_each_entry(iter, &g_ready_list, list) {
		if (iter->mpidr == (uint64_t)-1) {
			new_task = iter;
			break;
		}
	}
	if (!new_task) {
		hyper_warn("psci_vcpu_on: cannot find freshly-created task to bind mpidr\n");
		return PSCI_RET_DENIED;
	}

	new_task->mpidr = target_mpidr;
	if (new_task->vcpu) {
		new_task->vcpu->arch.vmpidr = 0x80000000ULL | target_mpidr;
		uint64_t primary_vbar;
		asm volatile("mrs %0, vbar_el1" : "=r"(primary_vbar));
		new_task->vcpu->arch.vbar = primary_vbar;
		hyper_info("psci_vcpu_on: secondary vbar_el1=0x%lx\n", primary_vbar);
	}
	vcpu_reg_write(&new_task->vcpu->regs, 0, 0, context_id);
	hyper_info("psci_vcpu_on: bound task %d to mpidr 0x%lx\n",
	         new_task->id, new_task->mpidr);

	return PSCI_RET_SUCCESS;
}

static int __attribute__((unused)) psci_vcpu_call(struct cpu_user_regs *regs)
{
	// emulate_psci_set_reg(vcpu, regs, 0, 0x84000003);
	unsigned long psci_fn =
			vcpu_reg_read(regs, 0, 0);
	unsigned long val;

	switch (psci_fn) {
	case PSCI_0_2_FN_PSCI_VERSION:
		/*
		 * Bits[31:16] = Major Version = 0
		 * Bits[15:0] = Minor Version = 2
		 */
		val = 2;
		break;
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
	// 	val = psci_vcpu_suspend(vcpu, regs);
		break;
	case PSCI_0_2_FN_CPU_OFF:
	// 	psci_vcpu_off(vcpu, regs);
	// 	val = PSCI_RET_SUCCESS;
		break;
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
		val = psci_vcpu_on(regs);
		break;
	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
	// 	val = psci_vcpu_affinity_info(vcpu, regs);
		break;
	case PSCI_0_2_FN_MIGRATE:
	case PSCI_0_2_FN64_MIGRATE:
	// 	val = PSCI_RET_NOT_SUPPORTED;
	// 	break;
	case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
		/*
		 * Trusted OS is MP hence does not require migration
	         * or
		 * Trusted OS is not present
		 */
		val = PSCI_0_2_TOS_MP;
		break;
	case PSCI_0_2_FN_MIGRATE_INFO_UP_CPU:
	case PSCI_0_2_FN64_MIGRATE_INFO_UP_CPU:
	// 	val = PSCI_RET_NOT_SUPPORTED;
		break;
	case PSCI_0_2_FN_SYSTEM_OFF:
	// 	psci_system_off(vcpu, regs);
	// 	/*
	// 	 * We should'nt be going back to guest VCPU after
	// 	 * receiving SYSTEM_OFF request.
	// 	 *
	// 	 * If we accidently resume guest VCPU after SYSTEM_OFF
	// 	 * request then guest VCPU should see internal failure
	// 	 * from PSCI return value. To achieve this, we preload
	// 	 * r0 (or x0) with PSCI return value INTERNAL_FAILURE.
	// 	 */
	// 	val = PSCI_RET_INTERNAL_FAILURE;
		break;
	case PSCI_0_2_FN_SYSTEM_RESET:
	// 	psci_system_reset(vcpu, regs);
	// 	/*
	// 	 * Same reason as SYSTEM_OFF for preloading r0 (or x0)
	// 	 * with PSCI return value INTERNAL_FAILURE.
	// 	 */
	// 	val = PSCI_RET_INTERNAL_FAILURE;
		break;
	default:
		hyper_err("unsupported fn: %lx\n", psci_fn);
		// return VMM_EINVALID;
        return -5;
	}

	vcpu_reg_write(regs, 0, 0, val);

    // return VMM_OK;
	return 0;
}