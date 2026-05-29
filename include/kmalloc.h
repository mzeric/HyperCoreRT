#pragma once

void *kmalloc(uint64_t size);
void kfree(void *ptr);

int init_kmalloc();
int init_kmap();
