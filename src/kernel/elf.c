#include <elf.h>
#include <fb/fb.h>
#include <fs/vfs.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <printf.h>
#include <string.h>
#include <tasking/scheduler.h>

#define ROUND_UP(__addr, __align) (((__addr) + (__align) - 1) & ~((__align) - 1))

#define ELF_RELOCATEABLE 1
#define ELF_EXECUTABLE 2

#define ELF_HEAD_LOAD 1

char elf_ident[4] = {0x7f, 'E', 'L', 'F'};

uint8_t elf_run_binary(char *name, char *path, proc_t *proc,
                       size_t time_slice) {
  fs_file_t file = vfs_get_info(path);
  uint8_t *buffer = kmalloc(file.file_size);
  vfs_read(path, 0, file.file_size, buffer);

  elf_header_t *header = (elf_header_t *)buffer;
  if (header->type != ELF_EXECUTABLE)
    return 1;
  if (memcmp((void *)header->identifier, elf_ident, 4))
    return 1;

  elf_prog_header_t *prog_header = (void *)(buffer + header->prog_head_off);

  for (size_t i = 0; i < header->prog_head_count; i++) {
    if (prog_header->type == ELF_HEAD_LOAD) {
      uintptr_t mem = (uintptr_t)pmalloc(
          ROUND_UP(prog_header->mem_size, PAGE_SIZE) / PAGE_SIZE);

      for (size_t j = 0;
           j < ROUND_UP(prog_header->mem_size, PAGE_SIZE) / PAGE_SIZE;
           j += PAGE_SIZE)
        vmm_map_page(proc->pagemap, mem + j, prog_header->virt_addr + j, 0b111);

      memset((void *)mem, 0, prog_header->mem_size);
      memcpy((void *)mem, (void *)((uint64_t)buffer + prog_header->offset),
             prog_header->file_size);
    }

    prog_header =
        (elf_prog_header_t *)((uint8_t *)prog_header + header->prog_head_size);
  }

  sched_create_thread(name, (uintptr_t)header->entry, time_slice, 1, 1, proc);

  return 0;
}
