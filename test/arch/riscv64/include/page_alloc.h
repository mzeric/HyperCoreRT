#pragma once
void init_page_allocator(void);

int alloc_pages(int order);

void free_pages(int pfn, int order);