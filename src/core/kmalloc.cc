#include "vmio.h"
#include "tlsf.h"
// #include "mm.h"
#include "page.h"
#include "arch_page.h"
#include "spin_lock.h"
#include "kmalloc.h"

static void *g_kmalloc_handler;
static void *g_kmalloc_heap;
static spinlock_t g_kmalloc_lock = { .lock = SPIN_UNLOCKED };

#define KMALLOC_HEAP_SIZE (0x1000000) /* 16MB */

void *alloc_mem_pool(uint64_t size) {
    size = (size + PAGE_SIZE - 1) & (PAGE_SIZE - 1);
    int fpn = alloc_pages_cnt(size >> PAGE_SHIFT);
    if (fpn < 0) {
        hyper_err("alloc page failed:0x%lx", size);
        return NULL;
    }

    return (void *)PAGE_VIR(fpn);
}

void free_mem_pool(void *ptr, uint64_t size) {
    int pfn = VIR_TO_FN(ptr);
    int n = size >> 12;
    if (pfn < 0 || n <= 0) {
        hyper_err("invalid addr:%p or size: 0x%lx", ptr, size);
        return;
    }

    free_pages(pfn, n);
}

void *kmalloc(uint64_t size) {
    arch_spin_lock(&g_kmalloc_lock);
    void *ptr = tlsf_malloc(g_kmalloc_handler, size);
    arch_spin_unlock(&g_kmalloc_lock);
    if ((uintptr_t)ptr < (uintptr_t)g_kmalloc_heap) {
        hyper_err("tlsf_malloc wired return :%p", ptr);
        // panic("kmalloc");
    }

    return ptr;
}

void kfree(void *ptr) {
    if (!ptr)
        return;
    arch_spin_lock(&g_kmalloc_lock);
    tlsf_free(g_kmalloc_handler, ptr);
    arch_spin_unlock(&g_kmalloc_lock);
}

int init_kmalloc() {
    uint64_t size = KMALLOC_HEAP_SIZE;
    g_kmalloc_heap = alloc_mem_pool(size);
    if (!g_kmalloc_heap) {
        hyper_fatal("no enough mem for kmalloc's init: 0x%lx", size);
        return -1;
    }

    g_kmalloc_handler = tlsf_create_with_pool(g_kmalloc_heap, size);
    if (g_kmalloc_handler == NULL) {
        hyper_fatal("kmalloc's allocator failed");
        return -1;
    }

    // hyper_info("-----------------:%p", g_kmalloc_heap);
    return 0;
}

void fini_kmalloc(void) {
    if (!g_kmalloc_heap)
        free_mem_pool(g_kmalloc_heap, KMALLOC_HEAP_SIZE);
}


static void default_walker(void *ptr, size_t size, int used, void *user) {
    u64 *p = (u64 *)user;

    if (used)
        p[0] = size;
    else
        p[1] = size;
}
void dump_kmalloc_status() {
    return;
    void *pool = tlsf_get_pool(g_kmalloc_handler);

    u64 arg[2] = {0, 0};
    tlsf_walk_pool(pool, default_walker, arg);

    hyper_info("kmalloc pool status: used:%lx, free:%lx  %.2f%%",
             arg[0],
             arg[1],
             (double)arg[0] / arg[1] * 100);
}


