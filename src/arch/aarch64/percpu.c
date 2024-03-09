#include "percpu.h"
#include "htypes.h"

uint64_t __per_cpu_offset[CONFIG_SMP_CPU_NUM];

void init_percpu_page() {

    for(int i = 0; i< CONFIG_SMP_CPU_NUM ; ++i) {
        __per_cpu_offset[i] = 0;
    }
}