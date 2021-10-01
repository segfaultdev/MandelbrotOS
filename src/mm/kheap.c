#include <lock.h>
#include <mm/kheap.h>
#include <mm/liballoc.h>
#include <mm/pmm.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ROUNDUP(A, B)                                                          \
  ({                                                                           \
    __typeof__(A) _a_ = A;                                                     \
    __typeof__(B) _b_ = B;                                                     \
    (_a_ + (_b_ - 1)) / _b_;                                                   \
  })

static volatile lock_t liballoc_slock = {0};

void *liballoc_alloc(int size) {
  void *ptr = pcalloc(ROUNDUP(size, PAGE_SIZE));
  ptr += PHYS_MEM_OFFSET;
  return ptr;
}

int liballoc_free_(void *ptr, int pages) {
  free_pages(ptr - PHYS_MEM_OFFSET, pages + 1);
  return 0;
}

int liballoc_lock() { return LOCK(liballoc_slock); }

int liballoc_unlock() {
  UNLOCK(liballoc_slock);
  return 0;
}

void *kmalloc(size_t size) { return liballoc_alloc(size); }

void kfree(void *ptr) { liballoc_free(ptr); }

void *krealloc(void *ptr, size_t size) { return liballoc_realloc(ptr, size); }

void *kcalloc(size_t size) {
  void *ptr = liballoc_alloc(size);
  memset(ptr, 0, size);
  return ptr;
}
