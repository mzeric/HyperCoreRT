/*


*/
#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "tlsf.h"

#define TOTAL_PAGES     (1024 * 1024) // 总共有1M个页 = 4G
#define BITS_PER_BYTE   8
#define BITS_PER_UINT32 (sizeof(uint32_t) * BITS_PER_BYTE)
#define BITMAP_SIZE     (TOTAL_PAGES / BITS_PER_UINT32)

static uint32_t g_page_allocator_bitmap[BITMAP_SIZE]; /* 1 GB need 32KB */

typedef struct bitmap {
    uint32_t data[0];
} bitmap_t;

/*
    TODO:
    * vmalloc, bitmaps
    * ioremap,ptw
    * kmalloc/tlsf_malloc
*/
// 初始化位图

static void *g_kmalloc_handler;

void *kmalloc(uint64_t size) { return tlsf_malloc(g_kmalloc_handler, size); }

void *kfree(void *ptr) {
    if (ptr)
        tlsf_free(g_kmalloc_handler, ptr);
}

void *init_kmalloc() {
    void    *_kmalloc_start = NULL;
    uint64_t size = 0x100000;
    return tlsf_create_with_pool(_kmalloc_start, size);
}

void init_bitmap(uint32_t *bitmap, uint64_t len) { memset(bitmap, 0, sizeof(len)); }

void create_bitmap() {}

void destroy_bitmap() {}

// 设置位
void set_bit(uint32_t *bitmap, int n) {
    bitmap[n / BITS_PER_UINT32] |= (1U << (n % BITS_PER_UINT32));
}

// 清除位
void clear_bit(uint32_t *bitmap, int n) {
    bitmap[n / BITS_PER_UINT32] &= ~(1U << (n % BITS_PER_UINT32));
}

// 检查位是否被设置
int is_bit_set(uint32_t *bitmap, int n) {
    return bitmap[n / BITS_PER_UINT32] & (1U << (n % BITS_PER_UINT32));
}

// 从addr开始查找连续bit_count个位为1的起始位置
int find_next_bits(uint32_t *bitmap, int addr, int bit_count) {
    if (bit_count == 0)
        return -1;
    for (int i = addr; i <= TOTAL_PAGES - bit_count; i++) {
        int count = 0;
        for (int j = i; j < TOTAL_PAGES && count < bit_count; j++) {
            if (!is_bit_set(bitmap, j)) {
                count++;
            } else {
                break; // 遇到0，中断当前搜索
            }
        }
        if (count == bit_count) {
            return i; // 找到连续bit_count个位为1的起始位置
        }
    }
    return -1; // 未找到
}

// 查找连续bit_count个位为1的起始位置
int find_bitmap_first_bits(uint32_t *bitmap, int bit_count) {
    return find_next_bits(bitmap, 0, bit_count); // 从位图的起始位置开始搜索
}

int init_page_allocator() {
    init_bitmap(g_page_allocator_bitmap, sizeof(g_page_allocator_bitmap));
    return 0;
}

// 分配多个连续页
int alloc_pages_cnt(int cnt) {
    int n = cnt;
    int start = find_bitmap_first_bits(g_page_allocator_bitmap, n);

    if (start < 0)
        return start;

    for (int i = start; i < start + n; i++) {
        set_bit(g_page_allocator_bitmap, i);
    }

    return start;
}

int alloc_pages(int order) { return alloc_pages_cnt(1ul << order); }

int alloc_one_page() { return alloc_pages_cnt(1); }

// 释放多个连续页
void free_pages(int start, int order) {
    int n = 1ul << order;
    for (int i = start; i < start + n; i++) {
        clear_bit(g_page_allocator_bitmap, i);
    }
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
    printf("Bitmap layout: \n");

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