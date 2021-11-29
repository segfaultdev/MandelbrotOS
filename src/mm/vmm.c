#include <lock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stddef.h>
#include <stdint.h>

static volatile lock_t vmm_lock = {0};
pagemap_t kernel_pagemap;

static uint64_t *get_next_level(uint64_t *table, size_t index, uint64_t flags) {
  uint64_t *ret = 0;
  uint64_t *entry = (void *)((uint64_t)table + PHYS_MEM_OFFSET) + index * 8;

  if ((entry[0] & 1) != 0)
    ret = (uint64_t *)(entry[0] & (uint64_t)~0xfff);
  else {
    ret = pmalloc(1);
    entry[0] = (uint64_t)ret | flags;
  }

  return ret;
}

void invalidate_tlb(pagemap_t *pagemap, uintptr_t virtual_address) {
  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
  if (cr3 == (uint64_t)pagemap->top_level)
    asm volatile("invlpg (%0)" : : "r"(virtual_address));
}

void vmm_map_page(pagemap_t *pagemap, uintptr_t physical_address,
                  uintptr_t virtual_address, uint64_t flags) {
  LOCK(pagemap->lock);

  size_t pml_entry4 = (size_t)(virtual_address & ((size_t)0x1ff << 39)) >> 39;
  size_t pml_entry3 = (size_t)(virtual_address & ((size_t)0x1ff << 30)) >> 30;
  size_t pml_entry2 = (size_t)(virtual_address & ((size_t)0x1ff << 21)) >> 21;
  size_t pml_entry1 = (size_t)(virtual_address & ((size_t)0x1ff << 12)) >> 12;

  uint64_t *pml3 = get_next_level(pagemap->top_level, pml_entry4, flags);
  uint64_t *pml2 = get_next_level(pml3, pml_entry3, flags);
  uint64_t *pml1 = get_next_level(pml2, pml_entry2, flags);

  *(uint64_t *)((uint64_t)pml1 + PHYS_MEM_OFFSET + pml_entry1 * 8) =
      physical_address | flags;

  invalidate_tlb(pagemap, virtual_address);

  UNLOCK(pagemap->lock);
}

void vmm_unmap_page(pagemap_t *pagemap, uintptr_t virtual_address) {
  LOCK(pagemap->lock);

  size_t pml4_entry = (virtual_address & ((uint64_t)0x1ff << 39)) >> 39;
  size_t pml3_entry = (virtual_address & ((uint64_t)0x1ff << 30)) >> 30;
  size_t pml2_entry = (virtual_address & ((uint64_t)0x1ff << 21)) >> 21;
  size_t pml1_entry = (virtual_address & ((uint64_t)0x1ff << 12)) >> 12;

  uint64_t *pml3 = get_next_level(pagemap->top_level, pml4_entry, 0b111);
  uint64_t *pml2 = get_next_level(pml3, pml3_entry, 0b111);
  uint64_t *pml1 = get_next_level(pml2, pml2_entry, 0b111);

  *(uint64_t *)((uint64_t)pml1[pml1_entry] + PHYS_MEM_OFFSET) = 0;

  invalidate_tlb(pagemap, virtual_address);

  UNLOCK(pagemap->lock);
}

void vmm_set_memory_flags(pagemap_t *pagemap, uintptr_t virtual_address,
                          uint64_t flags) {
  size_t pml4_entry = (virtual_address & ((uint64_t)0x1ff << 39)) >> 39;
  size_t pml3_entry = (virtual_address & ((uint64_t)0x1ff << 30)) >> 30;
  size_t pml2_entry = (virtual_address & ((uint64_t)0x1ff << 21)) >> 21;
  size_t pml1_entry = (virtual_address & ((uint64_t)0x1ff << 12)) >> 12;

  uint64_t *pml3 = get_next_level(pagemap->top_level, pml4_entry, 0b111);
  uint64_t *pml2 = get_next_level(pml3, pml3_entry, 0b111);
  uint64_t *pml1 = get_next_level(pml2, pml2_entry, 0b111);

  *(uint64_t *)((uint64_t)pml1[pml1_entry] + PHYS_MEM_OFFSET) &=
      ~(uint64_t)0xfff;

  *(uint64_t *)((uint64_t)pml1[pml1_entry] + PHYS_MEM_OFFSET) |= flags;
}

void vmm_load_pagemap(pagemap_t *pagemap) {
  asm volatile("mov %0, %%cr3" : : "a"(pagemap->top_level));
}

pagemap_t *create_new_pagemap() {
  pagemap_t *new_map = kcalloc(sizeof(pagemap_t));
  new_map->top_level = pcalloc(1);

  uint64_t *kernel_top =
      (uint64_t *)((void *)kernel_pagemap.top_level + PHYS_MEM_OFFSET);
  uint64_t *user_top =
      (uint64_t *)((void *)new_map->top_level + PHYS_MEM_OFFSET);

  for (uintptr_t i = 256; i < 512; i++)
    user_top[i] = kernel_top[i];

  return new_map;
}

int init_vmm() {
  kernel_pagemap.top_level = (uint64_t *)pcalloc(1);

  for (uint64_t i = 256; i < 512; i++)
    get_next_level(kernel_pagemap.top_level, i, 0b111);

  for (uintptr_t i = PAGE_SIZE; i < 0x100000000; i += PAGE_SIZE) {
    vmm_map_page(&kernel_pagemap, i, i, 0b111);
    vmm_map_page(&kernel_pagemap, i, i + PHYS_MEM_OFFSET, 0b111);
  }

  for (uintptr_t i = 0; i < 0x80000000; i += PAGE_SIZE)
    vmm_map_page(&kernel_pagemap, i, i + KERNEL_MEM_OFFSET, 0b111);

  vmm_load_pagemap(&kernel_pagemap);

  return 0;
}
