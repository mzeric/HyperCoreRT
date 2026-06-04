#include "riscv_fence_manager.h"

#include "arch_barrier.h"
#include "guest_dtb.h"
#include "ipi.h"
#include "mmu.h"
#include "riscv_ipi.h"
#include "riscv_sbi.h"
#include "sched.h"
#include "smp.h"

static void local_remote_fence_all(void) {
    fence_i();
    hfence();
}

static bool guest_hart_selected(u32 hart, u64 hart_mask, u64 hart_base) {
    if (hart_base == (u64)-1)
        return true;
    if (hart < hart_base)
        return false;

    u64 bit = hart - hart_base;
    if (bit >= 64)
        return false;
    return hart_mask & (1UL << bit);
}

long riscv_remote_fence(enum riscv_fence_kind kind, u64 hart_mask, u64 hart_base) {
    (void)kind;

    bool selected_pcpu[CONFIG_SMP_CPU_NUM] = {0};
    for (u32 hart = 0; hart < riscv_guest_vcpu_count(); hart++) {
        if (!guest_hart_selected(hart, hart_mask, hart_base))
            continue;

        hyper_task_t *task = riscv_find_guest_vcpu(hart);
        if (!task)
            continue;
        int pcpu = task->pcpu_affinity;
        if (pcpu < 0)
            pcpu = cpu_id();
        if (pcpu >= 0 && pcpu < CONFIG_SMP_CPU_NUM)
            selected_pcpu[pcpu] = true;
    }

    int me = cpu_id();
    long status = SBI_SUCCESS;
    for (int pcpu = 0; pcpu < smp_cpu_count() && pcpu < CONFIG_SMP_CPU_NUM; pcpu++) {
        if (!selected_pcpu[pcpu])
            continue;
        if (pcpu == me)
            local_remote_fence_all();
        else if (riscv_ipi_send_cpu_sync(pcpu, IPI_REMOTE_FENCE_ALL) != 0)
            status = SBI_ERR_FAILED;
    }

    return status;
}
