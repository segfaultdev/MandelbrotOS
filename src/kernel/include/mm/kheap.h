#ifndef __KHEAP_H__
#define __KHEAP_H__

#include <stddef.h>
#include <stdint.h>

void *kmalloc(size_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t size);
void *kcalloc(size_t size);
int kheap_init();

#endif
