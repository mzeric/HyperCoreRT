#include "percpu.h"
#include "htypes.h"
#include "page.h"
#include "arch_page.h"
#include "vmio.h"
#include <string.h>

unsigned long __per_cpu_offset[CONFIG_SMP_CPU_NUM];

void init_percpu_area(void)
{
    unsigned long per_cpu_size = __per_cpu_data_end - __per_cpu_start;

    /* CPU 0 uses the linker-placed template directly.
     * The offset must be the base address so that:
     *   __per_cpu_offset[0] + (var_addr - __per_cpu_start) = var_addr
     */
    __per_cpu_offset[0] = (unsigned long)__per_cpu_start;

    if (per_cpu_size == 0) {
        /* No DEFINE_PER_CPU variables yet — nothing to allocate. */
        return;
    }

    /* Allocate per-CPU copies for CPU 1..N-1 */
    for (int i = 1; i < CONFIG_SMP_CPU_NUM; i++) {
        int pfn = alloc_one_page();
        if (pfn < 0) {
            hyper_fatal("percpu alloc failed for cpu%d", i);
            return;
        }
        void *area = (void *)PAGE_VIR(pfn);
        memcpy(area, __per_cpu_start, per_cpu_size);
        __per_cpu_offset[i] = (unsigned long)area;
    }

    hyper_info("percpu area: %lu bytes/cpu, %d cpus", per_cpu_size, CONFIG_SMP_CPU_NUM);
}
