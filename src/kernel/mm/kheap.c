#include <lock.h>
#include <mm/kheap.h>
#include <mm/liballoc.h>
#include <mm/pmm.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ROUND_UP(A, B)                                                         \
  ({                                                                           \
    __typeof__(A) _a_ = A;                                                     \
    __typeof__(B) _b_ = B;                                                     \
    (_a_ + (_b_ - 1)) / _b_;                                                   \
  })

lock_t liballoc_slock = {0};

void *liballoc_alloc_pages(int size) {
  return (void *)((uintptr_t)pcalloc(size) + PHYS_MEM_OFFSET);
}

int liballoc_free_pages(void *ptr, size_t pages) {
  pmm_free_pages((void *)((uintptr_t)ptr - PHYS_MEM_OFFSET), pages);
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

void *kcalloc(size_t size) { return liballoc_calloc(size, 1); }
