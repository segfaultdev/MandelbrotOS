#include <lock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stddef.h>
#include <stdint.h>

static volatile lock_t vmm_lock = {0};

pagemap_t kernel_pagemap;

uint64_t orig_cr3;

static uint64_t *get_next_level(uint64_t *table, size_t index, uint64_t flags) {
  if (!(table[index] & 1)) {
    table[index] = (uint64_t)pcalloc(1);
    table[index] |= flags;
  }
  return (uint64_t *)((table[index] & ~(0x1ff)) + PHYS_MEM_OFFSET);
}

void invalidate_tlb(pagemap_t *pagemap, uintptr_t virtual_address) {
  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
  if ((cr3 & ~((uint64_t)0x1ff)) ==
      ((uint64_t)pagemap->top_level & ~((uint64_t)0x1ff)))
    asm volatile("invlpg (%0)" : : "r"(virtual_address));
}

void vmm_map_page(pagemap_t *pagemap, uintptr_t physical_address,
                  uintptr_t virtual_address, uint64_t flags) {
  LOCK(pagemap->lock);

  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
  if (cr3 != orig_cr3)
    asm volatile("mov %0, %%cr3" : : "a"(orig_cr3));

  size_t level4 = (size_t)(virtual_address & ((size_t)0x1ff << 39)) >> 39;
  size_t level3 = (size_t)(virtual_address & ((size_t)0x1ff << 30)) >> 30;
  size_t level2 = (size_t)(virtual_address & ((size_t)0x1ff << 21)) >> 21;
  size_t level1 = (size_t)(virtual_address & ((size_t)0x1ff << 12)) >> 12;

  uint64_t *pml3 = get_next_level(pagemap->top_level, level4, flags);
  uint64_t *pml2 = get_next_level(pml3, level3, flags);
  uint64_t *pml1 = get_next_level(pml2, level2, flags);

  pml1[level1] = physical_address | flags;

  invalidate_tlb(pagemap, virtual_address);

  UNLOCK(pagemap->lock);
}

void vmm_unmap_page(pagemap_t *pagemap, uintptr_t virtual_address,
                    uint64_t flags) {
  LOCK(pagemap->lock);

  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
  if (cr3 != orig_cr3)
    asm volatile("mov %0, %%cr3" : : "a"(orig_cr3));

  size_t level4 = (size_t)(virtual_address & ((size_t)0x1ff << 39)) >> 39;
  size_t level3 = (size_t)(virtual_address & ((size_t)0x1ff << 30)) >> 30;
  size_t level2 = (size_t)(virtual_address & ((size_t)0x1ff << 21)) >> 21;
  size_t level1 = (size_t)(virtual_address & ((size_t)0x1ff << 12)) >> 12;

  uint64_t *pml3 = get_next_level(pagemap->top_level, level4, flags);
  uint64_t *pml2 = get_next_level(pml3, level3, flags);
  uint64_t *pml1 = get_next_level(pml2, level2, flags);

  pml1[level1] = 0;

  invalidate_tlb(pagemap, virtual_address);

  UNLOCK(pagemap->lock);
}

void vmm_load_pagemap(pagemap_t *pagemap) {
  asm volatile("mov %0, %%cr3" : : "a"(pagemap->top_level));
}

pagemap_t *create_new_pagemap() {
  pagemap_t *new_map = kcalloc(sizeof(pagemap_t));
  new_map->top_level = pcalloc(1);
  return new_map;
}

int init_vmm() {
  asm volatile("mov %%cr3, %0" : "=r"(orig_cr3) : : "memory");

  kernel_pagemap.top_level = (uint64_t *)pcalloc(1);

  for (uintptr_t i = 0; i < 0x80000000; i += PAGE_SIZE)
    vmm_map_page(&kernel_pagemap, i, i + KERNEL_MEM_OFFSET, 0b111);

  for (uintptr_t i = 0; i < 0x200000000; i += PAGE_SIZE)
    vmm_map_page(&kernel_pagemap, i, i + PHYS_MEM_OFFSET, 0b111);

  vmm_load_pagemap(&kernel_pagemap);

  return 0;
}
