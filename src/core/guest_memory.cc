#include "htypes.h"
#include "compiler.h"
#include "vcpu.h"
#include "guest_memory.h"

void guest_mem_add_region(vcpu_t *vcpu, struct mem_region *mem) {
    list_add_tail(&mem->list, &vcpu->mem_region);
}

int is_overlapping(uint64_t gpa1, uint64_t size1, uint64_t gpa2, uint64_t size2) {
    uint64_t end1 = gpa1 + size1;
    uint64_t end2 = gpa2 + size2;
    return !(end1 <= gpa2 || gpa1 >= end2);
}

int check_overlap( vcpu_t *vcpu, struct mem_region *mem) {
    struct mem_region *pos = NULL;

    struct list_head *head = &vcpu->mem_region;

    uint64_t mem_gpa = mem->gpa;
    uint64_t mem_size = mem->size;
    // 遍历链表，检查重叠
    list_for_each_entry(pos, head, list) {
        if (is_overlapping(pos->gpa, pos->size, mem_gpa, mem_size)) {
            hyper_fatal("Overlapping memory region detected(%lx, %lx) vs (%lx,%lx)",
                      pos->gpa,
                      pos->size,
                      mem_gpa,
                      mem_size);
            return 1;
        }
    }

    return 0;
}

void guest_mem_insert_region(vcpu_t *vcpu, struct mem_region *mem) {
    struct mem_region *pos = NULL;

    struct list_head *head = &vcpu->mem_region;

    if (check_overlap(vcpu, mem)) {
        return;
    }

    // 遍历链表，找到合适的插入位置
    list_for_each_entry(pos, head, list) {
        if (pos->gpa <= mem->gpa && mem->gpa < (pos->gpa + pos->size)) {
            // 找到了位置
            // return container_of(pos, struct mem_region, list);
        }
    }
}

struct mem_region *guest_mem_find_region(vcpu_t *vcpu, uint64_t gpa, int attr) {
    struct mem_region *pos = NULL;

    struct list_head *head = &vcpu->mem_region;

    list_for_each_entry(pos, head, list) {
        if (pos->gpa <= gpa && gpa < (pos->gpa + pos->size)) {
            return pos;
        }
    }

    return NULL;
}



