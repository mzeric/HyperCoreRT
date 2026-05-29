#include "emul_gic.h"
#include "htypes.h"
#include "sched.h"
#include "sched_simple.h"
#include "src/drivers/gic/gicv3.h"
#include "sys_reg.h"

extern hyper_task_t *g_current_task;

/* Bit layout per ARM ARM D13.4.x for ICH_LR<n>_EL2 (64-bit). */
#define ICH_LR_VINTID(v)   ((u64)((v) & 0xffffffffULL))
#define ICH_LR_PINTID(p)   (((u64)((p) & 0x3ffULL)) << 32)
#define ICH_LR_GROUP1      (1ULL << 60)
#define ICH_LR_HW          (1ULL << 61)
#define ICH_LR_PEND        (1ULL << 62)
#define ICH_LR_ACT         (1ULL << 63)
#define ICH_LR_PRIO(p)     (((u64)((p) & 0xffULL)) << 48)

/* ICH_HCR_EL2 / ICH_VMCR_EL2 fields (avoid redef from gicv3.h) */
#define MY_ICH_HCR_EN      (1ULL << 0)
#define MY_ICH_VMCR_VENG1  (1ULL << 1)
#define MY_ICH_VMCR_VEOIM  (1ULL << 9)
#define MY_ICH_VMCR_VPMR(x) (((u64)((x) & 0xffULL)) << 24)

/* Numeric encodings (msr_s / mrs_s require integer operands). */
#define ENC_ICH_HCR_EL2     sys_reg(3, 4, 12, 11, 0)
#define ENC_ICH_VTR_EL2     sys_reg(3, 4, 12, 11, 1)
#define ENC_ICH_VMCR_EL2    sys_reg(3, 4, 12, 11, 7)
#define ENC_ICH_AP0R(n)     sys_reg(3, 4, 12, 8, (n))
#define ENC_ICH_AP1R(n)     sys_reg(3, 4, 12, 9, (n))
#define ENC_ICH_LR(n)       sys_reg(3, 4, 12, 12 + ((n) / 8), (n) & 7)

static u64 gic_emul_v3_read_lr(u32 lr)
{
	switch (lr) {
	case 0:  return mrs_s(ENC_ICH_LR(0));
	case 1:  return mrs_s(ENC_ICH_LR(1));
	case 2:  return mrs_s(ENC_ICH_LR(2));
	case 3:  return mrs_s(ENC_ICH_LR(3));
	case 4:  return mrs_s(ENC_ICH_LR(4));
	case 5:  return mrs_s(ENC_ICH_LR(5));
	case 6:  return mrs_s(ENC_ICH_LR(6));
	case 7:  return mrs_s(ENC_ICH_LR(7));
	case 8:  return mrs_s(ENC_ICH_LR(8));
	case 9:  return mrs_s(ENC_ICH_LR(9));
	case 10: return mrs_s(ENC_ICH_LR(10));
	case 11: return mrs_s(ENC_ICH_LR(11));
	case 12: return mrs_s(ENC_ICH_LR(12));
	case 13: return mrs_s(ENC_ICH_LR(13));
	case 14: return mrs_s(ENC_ICH_LR(14));
	case 15: return mrs_s(ENC_ICH_LR(15));
	default: hyper_err("%s: LR%d invalid\n", __func__, lr); break;
	}
	return 0;
}

static void gic_emul_v3_write_lr(u32 lr, u64 val)
{
	switch (lr) {
	case 0:  msr_s(ENC_ICH_LR(0),  val); return;
	case 1:  msr_s(ENC_ICH_LR(1),  val); return;
	case 2:  msr_s(ENC_ICH_LR(2),  val); return;
	case 3:  msr_s(ENC_ICH_LR(3),  val); return;
	case 4:  msr_s(ENC_ICH_LR(4),  val); return;
	case 5:  msr_s(ENC_ICH_LR(5),  val); return;
	case 6:  msr_s(ENC_ICH_LR(6),  val); return;
	case 7:  msr_s(ENC_ICH_LR(7),  val); return;
	case 8:  msr_s(ENC_ICH_LR(8),  val); return;
	case 9:  msr_s(ENC_ICH_LR(9),  val); return;
	case 10: msr_s(ENC_ICH_LR(10), val); return;
	case 11: msr_s(ENC_ICH_LR(11), val); return;
	case 12: msr_s(ENC_ICH_LR(12), val); return;
	case 13: msr_s(ENC_ICH_LR(13), val); return;
	case 14: msr_s(ENC_ICH_LR(14), val); return;
	case 15: msr_s(ENC_ICH_LR(15), val); return;
	default: hyper_err("%s: LR%d invalid\n", __func__, lr); return;
	}
}

static u32 g_vgic_lr_count = 4;
static u32 g_vgic_apr_count = 1;

static u64 gic_emul_v3_read_ap0r(u32 n)
{
	switch (n) {
	case 0: return mrs_s(ENC_ICH_AP0R(0));
	case 1: return mrs_s(ENC_ICH_AP0R(1));
	case 2: return mrs_s(ENC_ICH_AP0R(2));
	case 3: return mrs_s(ENC_ICH_AP0R(3));
	default: return 0;
	}
}

static u64 gic_emul_v3_read_ap1r(u32 n)
{
	switch (n) {
	case 0: return mrs_s(ENC_ICH_AP1R(0));
	case 1: return mrs_s(ENC_ICH_AP1R(1));
	case 2: return mrs_s(ENC_ICH_AP1R(2));
	case 3: return mrs_s(ENC_ICH_AP1R(3));
	default: return 0;
	}
}

static void gic_emul_v3_write_ap0r(u32 n, u64 val)
{
	switch (n) {
	case 0: msr_s(ENC_ICH_AP0R(0), val); return;
	case 1: msr_s(ENC_ICH_AP0R(1), val); return;
	case 2: msr_s(ENC_ICH_AP0R(2), val); return;
	case 3: msr_s(ENC_ICH_AP0R(3), val); return;
	default: return;
	}
}

static void gic_emul_v3_write_ap1r(u32 n, u64 val)
{
	switch (n) {
	case 0: msr_s(ENC_ICH_AP1R(0), val); return;
	case 1: msr_s(ENC_ICH_AP1R(1), val); return;
	case 2: msr_s(ENC_ICH_AP1R(2), val); return;
	case 3: msr_s(ENC_ICH_AP1R(3), val); return;
	default: return;
	}
}

static u32 gic_emul_lr_count(void)
{
	return g_vgic_lr_count;
}

static u32 gic_emul_apr_count(void)
{
	return g_vgic_apr_count;
}

static void gic_emul_probe_cpuif_caps(void)
{
	u64 vtr = mrs_s(ENC_ICH_VTR_EL2);
	u32 count = (vtr & 0x1f) + 1;
	g_vgic_lr_count = min(count, (u32)VCPU_MAX_VGIC_LRS);
	g_vgic_apr_count = 1;
}

static u64 gic_emul_default_vmcr(void)
{
	return MY_ICH_VMCR_VENG1 | MY_ICH_VMCR_VEOIM | MY_ICH_VMCR_VPMR(0xff);
}

void gic_vcpu_save(vcpu_t *vcpu)
{
	if (!vcpu)
		return;
	vcpu->arch.vgic.hcr = mrs_s(ENC_ICH_HCR_EL2);
	vcpu->arch.vgic.vmcr = mrs_s(ENC_ICH_VMCR_EL2);
	vcpu->arch.vgic.lr_count = gic_emul_lr_count();
	for (u32 i = 0; i < gic_emul_apr_count(); ++i) {
		vcpu->arch.vgic.ap0r[i] = gic_emul_v3_read_ap0r(i);
		vcpu->arch.vgic.ap1r[i] = gic_emul_v3_read_ap1r(i);
	}
	for (u32 i = 0; i < vcpu->arch.vgic.lr_count; ++i)
		vcpu->arch.vgic.lr[i] = gic_emul_v3_read_lr(i);
	vcpu->arch.vgic.initialized = true;
}

void gic_vcpu_restore(vcpu_t *vcpu)
{
	if (!vcpu)
		return;
	msr_s(ENC_ICH_HCR_EL2, 0);
	for (u32 i = 0; i < gic_emul_lr_count(); ++i)
		gic_emul_v3_write_lr(i, 0);
	if (vcpu->arch.vgic.initialized) {
		msr_s(ENC_ICH_VMCR_EL2, vcpu->arch.vgic.vmcr);
		for (u32 i = 0; i < gic_emul_apr_count(); ++i) {
			gic_emul_v3_write_ap0r(i, vcpu->arch.vgic.ap0r[i]);
			gic_emul_v3_write_ap1r(i, vcpu->arch.vgic.ap1r[i]);
		}
		for (u32 i = 0; i < min(vcpu->arch.vgic.lr_count, gic_emul_lr_count()); ++i)
			gic_emul_v3_write_lr(i, vcpu->arch.vgic.lr[i]);
		msr_s(ENC_ICH_HCR_EL2, vcpu->arch.vgic.hcr | MY_ICH_HCR_EN);
	} else {
		msr_s(ENC_ICH_VMCR_EL2, gic_emul_default_vmcr());
		for (u32 i = 0; i < gic_emul_apr_count(); ++i) {
			gic_emul_v3_write_ap0r(i, 0);
			gic_emul_v3_write_ap1r(i, 0);
		}
		msr_s(ENC_ICH_HCR_EL2, MY_ICH_HCR_EN);
	}
	asm volatile("isb" ::: "memory");
}

static u64 gic_emul_mpidr_affinity(u64 mpidr)
{
	return mpidr & 0xff00ffffffULL;
}

static u64 gic_emul_sgi_target_mpidr(const struct gic_vcpu_sgi *sgi, u32 aff0)
{
	return ((u64)sgi->aff3 << 32) |
	       ((u64)sgi->aff2 << 16) |
	       ((u64)sgi->aff1 << 8) |
	       aff0;
}

static int gic_emul_send_sgi_to_task(hyper_task_t *target, hyper_task_t *source,
                                 int irq)
{
	if (!target || target == source)
		return 0;
	gic_vcpu_inject_virq(target, irq);
	return 1;
}

int gic_vcpu_send_sgi(const struct gic_vcpu_sgi *sgi) {
	if (!sgi)
		return -1;

	int delivered = 0;
	hyper_task_t *source = current_task();

	if (sgi->irm) {
		delivered += gic_emul_send_sgi_to_task(g_current_task, source, sgi->intid);
		hyper_task_t *target;
		list_for_each_entry(target, &g_ready_list, list)
			delivered += gic_emul_send_sgi_to_task(target, source, sgi->intid);
	} else {
		for (u32 bit = 0; bit < 16; ++bit) {
			if (!(sgi->target_list & (1U << bit)))
				continue;
			u32 aff0 = ((u32)sgi->rs * 16U) + bit;
			hyper_task_t *target = find_task_by_mpidr(gic_emul_sgi_target_mpidr(sgi, aff0));
			if (!target) {
				hyper_warn("vgic: SGI%d target mpidr 0x%lx not found\n",
				         sgi->intid, gic_emul_sgi_target_mpidr(sgi, aff0));
				continue;
			}
			gic_vcpu_inject_virq(target, sgi->intid);
			delivered++;
		}
	}

	if (!delivered)
		hyper_warn("vgic: SGI%d delivered to nobody\n", sgi->intid);
	return 0;
}

void gic_vcpu_init_pcpu(void) {
	gic_emul_probe_cpuif_caps();
	msr_s(ENC_ICH_VMCR_EL2, gic_emul_default_vmcr());
	for (u32 i = 0; i < gic_emul_apr_count(); ++i) {
		gic_emul_v3_write_ap0r(i, 0);
		gic_emul_v3_write_ap1r(i, 0);
	}
	for (u32 i = 0; i < gic_emul_lr_count(); ++i)
		gic_emul_v3_write_lr(i, 0);
	msr_s(ENC_ICH_HCR_EL2, MY_ICH_HCR_EN);
	asm volatile("isb" ::: "memory");
}

void gic_vcpu_inject_virq(hyper_task_t *task, int virq) {
	if (!task)
		return;
	if (task->pending_virq_count >= VCPU_MAX_PENDING_VIRQ) {
		hyper_warn("vgic: pending virq full on task %d, dropping irq %d\n",
		         task->id, virq);
		return;
	}
	/* De-dup: if already queued, don't double-pend. */
	for (int i = 0; i < task->pending_virq_count; ++i) {
		if (task->pending_virq[i] == virq)
			return;
	}
	task->pending_virq[task->pending_virq_count++] = virq;
}

void gic_vcpu_flush_lr(hyper_task_t *task) {
	if (!task)
		return;
	int keep = 0;
	for (int i = 0; i < task->pending_virq_count; ++i) {
		int slot = -1;
		for (u32 lr_idx = 0; lr_idx < gic_emul_lr_count(); ++lr_idx) {
			u64 old_lr = gic_emul_v3_read_lr(lr_idx);
			if (!(old_lr & (ICH_LR_PEND | ICH_LR_ACT))) {
				slot = lr_idx;
				break;
			}
		}
		if (slot < 0) {
			task->pending_virq[keep++] = task->pending_virq[i];
			continue;
		}

		int virq = task->pending_virq[i];
		u64 lr = ICH_LR_VINTID(virq) | ICH_LR_GROUP1 |
		         ICH_LR_PEND | ICH_LR_PRIO(0xa0);
		/* Keep HW=0 so guest EOI never deactivates a physical interrupt. */
		gic_emul_v3_write_lr(slot, lr);
	}
	asm volatile("isb" ::: "memory");
	task->pending_virq_count = keep;
}

hyper_task_t *find_task_by_mpidr(uint64_t mpidr) {
	mpidr = gic_emul_mpidr_affinity(mpidr);
	if (g_current_task && gic_emul_mpidr_affinity(g_current_task->mpidr) == mpidr)
		return g_current_task;
	hyper_task_t *t;
	list_for_each_entry(t, &g_ready_list, list) {
		if (gic_emul_mpidr_affinity(t->mpidr) == mpidr)
			return t;
	}
	return NULL;
}