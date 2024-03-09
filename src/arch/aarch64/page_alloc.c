/*


*/
#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "tlsf.h"
#include "bitmap.h"
#include "excep.h"
#include <string.h>

#define TOTAL_PAGES (1024 * 1024) // 总共有1M个页 = 4G


#define BITMAP_SIZE (TOTAL_PAGES / BITS_PER_UINT64)

static uint64_t g_page_allocator_bitmap[BITMAP_SIZE]; /* 1 GB need 32KB */

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

static void *g_kmalloc_handler;
static void *g_kmalloc_heap;
#define KMALLOC_HEAP_SIZE (0x1000000) /* 16MB */

void *kmalloc(uint64_t size) {
    void *ptr = tlsf_malloc(g_kmalloc_handler, size);
    if (ptr < g_kmalloc_heap) {
        vmm_err("tlsf_malloc wired return :%p\n", ptr);
        panic("kmalloc");
    }

    return ptr;
}

void *kfree(void *ptr) {
    if (ptr)
        tlsf_free(g_kmalloc_handler, ptr);
}

int init_kmalloc() {
    uint64_t size = KMALLOC_HEAP_SIZE;
    g_kmalloc_heap = alloc_mem_pool(size);
    if (!g_kmalloc_heap) {
        vmm_fatal("no enough mem for kmalloc's init: 0x%lx\n", size);
        return -1;
    }

    g_kmalloc_handler = tlsf_create_with_pool(g_kmalloc_heap, size);
    if (g_kmalloc_handler == NULL) {
        vmm_fatal("kmalloc's allocator failed\n");
        return -1;
    }

    vmm_info("-----------------:%p\n", g_kmalloc_heap);
    return 0;
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

    vmm_info("kmalloc pool status: used:%lx, free:%lx  %.2f%%\n",
             arg[0],
             arg[1],
             (double)arg[0] / arg[1] * 100);
}

void *fini_kmalloc() {
    if (!g_kmalloc_heap)
        free_mem_pool(g_kmalloc_heap, KMALLOC_HEAP_SIZE);
}

void init_bitmap(uint64_t *bitmap, uint64_t len) { memset(bitmap, 0, sizeof(len)); }

bitmap_t create_bitmap(uint64_t start, int ele_size, int bit_nr) {
    bitmap_t b = (bitmap_t){
        .bit_nr = bit_nr,
        .ele_size = ele_size,
        .start = start,
    };

    b.data = (uint64_t *)kmalloc(bit_nr / BITS_PER_BYTE);
    vmm_debug("kmalloc ptr:%p\n", b.data);
    memset(b.data, 0, bit_nr / BITS_PER_BYTE);
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

int map_one_frame(vaddr_t vir, paddr_t phy, int attr) {

    /* phy should not inside page-stack */
    // FIXME:check phy's pfn

    if (phy & (PAGE_SIZE - 1))
        vmm_fatal("phy addr not aligned: %lx\n", phy);

    return 0;
}

int __kmap_one_page(vaddr_t vir, paddr_t phy, int attr) {
    extern lpae_t boot_pgtable[];
    /* only alloc_one_page */
    __ptw_map_4k_page(vir, phy, boot_pgtable, 0, attr);

    return 0;
}

int __kmap_pages(vaddr_t vir, paddr_t phy, int page_num, int attr) {
    for(int i = 0; i < page_num; ++i){
        __kmap_one_page(vir, phy, attr);
        vir += PAGE_SIZE;
        phy += PAGE_SIZE;
    }

    return 0;
}

void *ioremap_page(paddr_t phy, int attr) {
    u64 vaddr = __vmalloc(PAGE_SIZE);
    if (!vaddr)
        vmm_fatal("vmalloc failed\n");

    vmm_debug("ioremap <%p, %p>\n", vaddr, phy);
    __kmap_one_page(vaddr, phy, attr);

    return (void *)vaddr;
}

void *ioremap(paddr_t phy, int size, int attr) {
    int page_num = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;
    u64 vaddr = __vmalloc(size);
    if (!vaddr)
        vmm_fatal("vmalloc failed\n");

    vmm_debug("ioremap <%p, %p>\n", vaddr, phy);
    __kmap_pages(vaddr, phy, page_num, attr);

    return (void *)vaddr;
}

void iounmap_page(vaddr_t vir) {
    extern lpae_t boot_pgtable[];

    __ptw_unmap_4k_page(vir, boot_pgtable, 0);
}

void iounmap(vaddr_t vir, int size) {
    int page_num = round_up(size, PAGE_SIZE) >> PAGE_SHIFT;
    for (int i = 0; i < page_num; ++i) {
        iounmap_page(vir);
        vir += PAGE_SIZE;
    }
}

void page_alloc_test() {}

int init_page_allocator() {
    init_bitmap(g_page_allocator_bitmap, sizeof(g_page_allocator_bitmap));
    return 0;
}

// 分配多个连续页
int alloc_pages_cnt(int cnt) {
    int n = cnt;
    int start = bitmap_find_next_zero_area(g_page_allocator_bitmap, TOTAL_PAGES, 0, n, 1);
    if (start >= TOTAL_PAGES)
        return TOTAL_PAGES;


    set_bits(g_page_allocator_bitmap, start, n);
    // vmm_info("alloc-page: %d -> :%d\n", start, cnt);
    return start;
}

int alloc_pages(int order) { return alloc_pages_cnt(1ul << order); }

int alloc_one_page() { return alloc_pages_cnt(1); }

void free_one_page(int pfn) { free_pages_cnt(pfn, 1); }

void free_pages_cnt(int pfn, int cnt) {
    // vmm_info("free-page: %x -> :%d\n", pfn, cnt);
    clear_bits(g_page_allocator_bitmap, pfn, cnt);
}

void free_pages(int pfn, int order) { free_pages_cnt(pfn, 1ul << order); }

void *alloc_mem_pool(uint64_t size) {
    size = (size + PAGE_SIZE - 1) & PAGE_MASK;
    int fpn = alloc_pages_cnt(size >> PAGE_SHIFT);
    if (fpn < 0) {
        vmm_err("alloc page failed:0x%x\n", size);
        return NULL;
    }

    return PAGE_VIR(fpn);
}

void free_mem_pool(void *ptr, uint64_t size) {
    int pfn = VIR_TO_FN(ptr);
    int n = size >> 12;
    if (pfn < 0 || n <= 0) {
        vmm_err("invalid addr:%p or size: 0x%lx\n", ptr, size);
        return;
    }

    free_pages(pfn, n);
}

void page_summary() {
    int allocatedPages = 0;
    for (int i = 0; i < TOTAL_PAGES; i++) {
        if (is_bit_set(g_page_allocator_bitmap, i)) {
            allocatedPages++;
        }
    }
    double allocatedPercentage = (double)allocatedPages / TOTAL_PAGES * 100;

    vmm_info("Allocated pages: %d/%d\n", allocatedPages, TOTAL_PAGES);
    vmm_info("Allocated percentage: %.2f%%\n", allocatedPercentage);
}

void print_page_layout(int start, int end) {
    // 打印位图布局
    printf("Bitmap layout: <%lx - %lx>\n", start, end);

    for (int i = start; i < end; i++) {
        printf("%d", is_bit_set(g_page_allocator_bitmap, i) ? 1 : 0);
        if ((i + 1) % 64 == 0) { // 每64位换一行，以便于阅读
            printf("\n");
        } else if ((i + 1) % 8 == 0) { // 每8位添加一个空格，增加可读性
            printf(" ");
        }
    }
    printf("\n");
}