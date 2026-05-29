#pragma once
#include "emul_dev.h"
#include "vcpu.h"

struct mem_region {
    uint64_t         gpa;
    uint64_t         hpa;
    uint64_t         size;
    int32_t          attr;
    struct emul_device   *dev;
    char             match_name[EMUL_DEV_MAX_MATCH_NAME];
    struct list_head list;
};

void               guest_mem_add_region(vcpu_t *vcpu, struct mem_region *mem);
struct mem_region *guest_mem_find_region(vcpu_t *vcpu, uint64_t gpa, int attr);