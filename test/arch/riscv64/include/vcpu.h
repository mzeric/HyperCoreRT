#pragma once
#include "htypes.h"
#include "arch_regs.h"
#include "list.h"

struct arch_vcpu {
    uint64_t stack;
};

typedef struct vcpu {

    int vcpu_id;
    int phys_id;
    int priority;
    struct list_head     list;


    struct arch_vcpu     arch;
    struct cpu_arch      carch;
    struct cpu_user_regs regs;

}vcpu_t;
