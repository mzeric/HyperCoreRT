
#include "bitmap.h"
#include "htypes.h"
#include "safe_printf.h"
#include "vmio.h"
#include "arch_page.h"

#define TOTAL_PAGES (1024 * 1024) // 总共有1M个页 = 4G


#define BITMAP_SIZE (TOTAL_PAGES / BITS_PER_UINT64)


static uint64_t g_page_allocator_bitmap[BITMAP_SIZE]; /* 1 GB need 32KB */

int alloc_pages_cnt(int cnt) {
    int start = bitmap_find_next_zero_area(g_page_allocator_bitmap, TOTAL_PAGES, 0, cnt, 1);

    if (start >= TOTAL_PAGES)
        return TOTAL_PAGES;

    set_bits(g_page_allocator_bitmap, start, cnt);

    return start;
}

void free_pages_cnt(int pfn, int cnt) { clear_bits(g_page_allocator_bitmap, pfn, cnt); }

int alloc_pages(int order) { return alloc_pages_cnt(1ul << order); }

int alloc_one_page() { return alloc_pages_cnt(1); }

void free_one_page(int pfn) { free_pages_cnt(pfn, 1); }

void free_pages(int pfn, int order) { free_pages_cnt(pfn, 1ul << order); }

int init_page_allocator(void) {
    init_bitmap(g_page_allocator_bitmap, sizeof(g_page_allocator_bitmap));
    return 0;
}

paddr_t vir_to_phy(vaddr_t v) {
    if (v < PAGE_VIRT_OFFSET)
        return (paddr_t)v;

    return (paddr_t)PAGE_VIRT_TO_PHYS(v);
}

vaddr_t phy_to_vir(paddr_t v) {
    if (v < PAGE_PHYS_OFFSET)
        return v;

    return PAGE_PHYS_TO_VIRT(v);
}

void page_summary() {
    int allocatedPages = 0;
    for (int i = 0; i < TOTAL_PAGES; i++) {
        if (is_bit_set(g_page_allocator_bitmap, i)) {
            allocatedPages++;
        }
    }
    double allocatedPercentage = (double)allocatedPages / TOTAL_PAGES * 100;

    hyper_info("Allocated pages: %d/%d", allocatedPages, TOTAL_PAGES);
    hyper_info("Allocated percentage: %.2f%%", allocatedPercentage);
}

void print_page_layout(int start, int end) {
    // 打印位图布局
    safe_printf("Bitmap layout: <%d - %d>\n", start, end);

    for (int i = start; i < end; i++) {
        safe_printf("%d", is_bit_set(g_page_allocator_bitmap, i) ? 1 : 0);
        if ((i + 1) % 64 == 0) { // 每64位换一行，以便于阅读
            safe_printf("\n");
        } else if ((i + 1) % 8 == 0) { // 每8位添加一个空格，增加可读性
            safe_printf(" ");
        }
    }
    safe_printf("\n");
}