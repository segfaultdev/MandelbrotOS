#include <lock.h>
#include <mm/kheap.h>
#include <mm/liballoc.h>
#include <mm/pmm.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ROUND_UP(__addr, __align) (((__addr) + (__align)-1) & ~((__align)-1))

lock_t liballoc_slock = {0};

void *liballoc_alloc(int size) {
  return (void *)((uintptr_t)pcalloc(ROUND_UP(size, PAGE_SIZE)) + PHYS_MEM_OFFSET);
}

int liballoc_free_(void *ptr, int pages) {
  pmm_free_pages(ptr - PHYS_MEM_OFFSET, pages + 1);
  return 0;
}

int liballoc_lock() { return LOCK(liballoc_slock); }

int liballoc_unlock() {
  UNLOCK(liballoc_slock);
  return 0;
}

void *kmalloc(size_t size) { return liballoc_malloc(size); }

void kfree(void *ptr) { liballoc_free(ptr); }

void *krealloc(void *ptr, size_t size) { return liballoc_realloc(ptr, size); }

void *kcalloc(size_t size) {
  void *ptr = liballoc_alloc(size);
  memset(ptr, 0, size);
  return ptr;
}
