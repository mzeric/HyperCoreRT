/*


*/
#include "vmio.h"
#include "page.h"
#include "arch_page.h"
#include "tlsf.h"
#include "bitmap.h"
#include "kmalloc.h"
#include <string.h>
#include <ioremap.h>

// #define TOTAL_PAGES (1024 * 1024) // 总共有1M个页 = 4G


// #define BITMAP_SIZE (TOTAL_PAGES / BITS_PER_UINT64)

// uint64_t g_page_allocator_bitmap[BITMAP_SIZE]; /* 1 GB need 32KB */

typedef struct bitmap {
    uint64_t *data;
    uint64_t  start;
    // end = start + bit_nr * ele_size;
    int      ele_size;
    uint64_t bit_nr;
} bitmap_t;

/*
    TODO:
    * vmalloc, bitmaps
    * ioremap,ptw
    * kmalloc/tlsf_malloc - DONE
*/
// 初始化位图


bitmap_t create_bitmap(uint64_t start, int ele_size, int bit_nr) {
    bitmap_t b = (bitmap_t){
        .bit_nr = bit_nr,
        .ele_size = ele_size,
        .start = start,
    };

    b.data = (uint64_t *)kmalloc(bit_nr / BITS_PER_BYTE);
    hyper_debug("kmalloc ptr:%p, %d, %d", b.data, bit_nr, bit_nr/BITS_PER_BYTE);
    memset(b.data, 0, 4567);
    safe_printf("here\n");
    return b;
}

void destroy_bitmap(bitmap_t map) {
    if (map.data)
        kfree(map.data);
}

uint64_t bitmap_alloc_range(bitmap_t bt, size_t size) {
    uint64_t cnt = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;
    uint64_t idx = bitmap_find_next_zero_area(bt.data, bt.bit_nr, 0, cnt, 0);
    set_bits(bt.data, idx, cnt);

    return bt.start + idx * bt.ele_size;
}

void bitmap_free_range(bitmap_t bt, uint64_t start, uint64_t size) {
    int      idx = (start - bt.start) / bt.ele_size;
    uint64_t cnt = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;
    clear_bits(bt.data, idx, cnt);
}

static bitmap_t g_vmalloc_bitmap;

int init_kmap() {

    int bitmap_bnr = (KMAP_VIRT_END - KMAP_VIRT_START) >> PAGE_SHIFT;

    g_vmalloc_bitmap = create_bitmap(KMAP_VIRT_START, PAGE_SIZE, bitmap_bnr);

    return 0;
}

void fini_kmap() { destroy_bitmap(g_vmalloc_bitmap); }

uint64_t __vmalloc(size_t size) { return bitmap_alloc_range(g_vmalloc_bitmap, size); }

void __vfree(uint64_t ptr, size_t size) {
    bitmap_free_range(g_vmalloc_bitmap, (uintptr_t)ptr, size);
}

int host_map_pages(vaddr_t vir, paddr_t phy, int page_num, int attr);
void host_unmap_pages(vaddr_t vir, int size);

void *ioremap_page(paddr_t phy, int attr) {
    u64 vaddr = __vmalloc(PAGE_SIZE);
    if (!vaddr)
        hyper_fatal("vmalloc failed");

    hyper_debug("ioremap <%lx, %lx>", vaddr, phy);
    host_map_one_page(vaddr, phy, attr);

    return (void *)vaddr;
}

void *ioremap(paddr_t phy, int size, int attr) {
    int page_num = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;
    u64 vaddr = __vmalloc(size);
    if (!vaddr)
        hyper_fatal("vmalloc failed");

    hyper_debug("ioremap <%lx, %lx>", vaddr, phy);
    host_map_pages(vaddr, phy, page_num, attr);

    return (void *)vaddr;
}

void iounmap(vaddr_t vir, int size) { host_unmap_pages(vir, size); }

void iounmap_page(vaddr_t vir) { host_unmap_one_page(vir); }


int host_map_pages(vaddr_t vir, paddr_t phy, int page_num, int attr) {
    for (int i = 0; i < page_num; ++i) {
        host_map_one_page(vir, phy, attr);
        vir += PAGE_SIZE;
        phy += PAGE_SIZE;
    }

    return 0;
}

void host_unmap_pages(vaddr_t vir, int size) {
    int page_num = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;
    for (int i = 0; i < page_num; ++i) {
        host_unmap_one_page(vir);
        vir += PAGE_SIZE;
    }
}
