#include "percpu.h"
#include "smp.h"
#include "htypes.h"
#include "kmalloc.h"
#include <string.h>

unsigned long __per_cpu_offset[CONFIG_SMP_CPU_NUM];

void init_percpu_area(void) {
    extern char __per_cpu_start[];
    extern char __per_cpu_end[];
    unsigned long percpu_size = (unsigned long)(__per_cpu_end - __per_cpu_start);

    if (percpu_size == 0)
        return;

    /* Set up per-CPU offsets — each CPU gets a copy of the percpu area */
    for (int i = 0; i < CONFIG_SMP_CPU_NUM; i++) {
        void *p = kmalloc(percpu_size);
        if (p) {
            memcpy(p, __per_cpu_start, percpu_size);
            __per_cpu_offset[i] = (unsigned long)p;
        }
    }
}
