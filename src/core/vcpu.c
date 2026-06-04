#include "arch_ops.h"
#include "kmalloc.h"
#include "vcpu.h"
#include "vmio.h"
#include <string.h>

vcpu_t *create_vcpu(int vcpu_id, int priority) {
    vcpu_t *vcpu = (vcpu_t *)kmalloc(sizeof(vcpu_t));
    if (!vcpu) {
        hyper_err("alloc vcpu struct failed");
        return NULL;
    }

    memset(vcpu, 0, sizeof(vcpu_t));
    INIT_LIST_HEAD(&vcpu->list);
    vcpu->vcpu_id = vcpu_id;
    vcpu->priority = priority;

    if (g_arch_ops.vcpu_create && g_arch_ops.vcpu_create(vcpu) != 0) {
        kfree(vcpu);
        return NULL;
    }

    return vcpu;
}

void destroy_vcpu(vcpu_t *vcpu) {
    if (!vcpu)
        return;

    if (g_arch_ops.vcpu_destroy)
        g_arch_ops.vcpu_destroy(vcpu);

    kfree(vcpu);
}
