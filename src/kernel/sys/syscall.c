#include <cpu_locals.h>
#include <drivers/ahci.h>
#include <drivers/apic.h>
#include <elf.h>
#include <fb/fb.h>
#include <fs/vfs.h>
#include <lock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <printf.h>
#include <registers.h>
#include <stdint.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <sys/syscall.h>

#define ABS(x) ((x < 0) ? (-x) : x)

#define MAX_HEAP_SIZE 0x10000
#define CURRENT_PAGEMAP get_locals()->current_thread->mother_proc->pagemap
#define CURRENT_PROC get_locals()->current_thread->mother_proc

typedef signed long ssize_t;

extern void syscall_isr();

lock_t syscall_lock = {0};

int init_syscalls() {
  idt_set_gate(&idt[0x45], 1, 0, syscall_isr);
  return 0;
}

size_t syscall_open(char *path) {
  uintptr_t kern_path_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)path);

  fs_file_t file = vfs_get_info((char *)kern_path_addr);
  if (!file.is_valid)
    return -1;

  syscall_file_t sfile = (syscall_file_t){
      .size = file.file_size,
      .offset = 0,
      .buffer = kmalloc(file.file_size),
      .file = file,
      .path = kmalloc(strlen(path)),
      .index = CURRENT_PROC->fds.length,
  };
  strcpy(sfile.path, path);

  vfs_read(path, 0, file.file_size, sfile.buffer);

  vec_push(&CURRENT_PROC->fds, sfile);

  return (size_t)CURRENT_PROC->fds.length - 1;
}

void syscall_close(size_t id) {
  if (id > (size_t)CURRENT_PROC->fds.length - 1)
    return;

  syscall_file_t *file = &CURRENT_PROC->fds.data[id];

  vfs_write(file->path, 0, file->size, file->buffer);

  kfree(file->buffer);
  kfree(file->path);
}

void syscall_read(size_t id, uint8_t *buffer, size_t size) {
  if (id > (size_t)CURRENT_PROC->fds.length - 1)
    return;

  syscall_file_t *file = &CURRENT_PROC->fds.data[id];

  if (size > file->size - file->offset)
    size = file->size - file->offset;

  vmm_memcpy(CURRENT_PAGEMAP, (uintptr_t)buffer, &kernel_pagemap,
             (uintptr_t)(file->buffer + file->offset), file->size);

  file->offset += size;
}

void syscall_write(size_t id, uint8_t *buffer, size_t size) {
  if (id > (size_t)CURRENT_PROC->fds.length - 1)
    return;

  syscall_file_t *file = &CURRENT_PROC->fds.data[id];

  if (size > file->size - file->offset) {
    file->buffer = krealloc(file->buffer, size + file->offset);
    file->size = size + file->offset;
  }

  vmm_memcpy(&kernel_pagemap, (uintptr_t)(file->buffer + file->offset),
             CURRENT_PAGEMAP, (uintptr_t)buffer, size);

  file->offset += size;
}

void syscall_seek(size_t id, size_t offset, int mode) {
  if (id > (size_t)CURRENT_PROC->fds.length - 1)
    return;

  syscall_file_t *file = &CURRENT_PROC->fds.data[id];

  switch (mode) {
    case SEEK_ZERO:
      file->offset = offset;
      break;
    case SEEK_FORWARDS:
      file->offset += offset;
      break;
    case SEEK_BACKWARDS:
      if (offset > file->offset)
        file->offset = 0;
      else
        file->offset -= offset;
      break;
  }
}

void syscall_exec(char *name, char *path, uint64_t arg1, uint64_t arg2,
                  uint64_t arg3) {
  uintptr_t name_kern_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)name) +
      PHYS_MEM_OFFSET;
  uintptr_t path_kern_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)path) +
      PHYS_MEM_OFFSET;

  proc_t *new_proc = sched_create_proc((char *)name_kern_addr, 1);

  elf_run_binary((char *)name_kern_addr, (char *)path_kern_addr, new_proc,
                 STANDARD_TIME_SLICE, arg1, arg2, arg3);
}

uint64_t syscall_sbrk(size_t size) {
  if (CURRENT_PROC->heap_size + size > MAX_HEAP_SIZE)
    return 0;

  if (CURRENT_PROC->heap_capacity < size) {
    void *new_mem = pmalloc(size / PAGE_SIZE + PAGE_SIZE);

    for (uintptr_t i = 0; i < size / PAGE_SIZE + PAGE_SIZE; i += PAGE_SIZE)
      vmm_map_page(CURRENT_PAGEMAP, (uintptr_t)new_mem + PHYS_MEM_OFFSET + i, (uintptr_t)CURRENT_PROC->heap + CURRENT_PROC->heap_capacity + i, 0b111);

    CURRENT_PROC->heap_capacity += (size / PAGE_SIZE + PAGE_SIZE);
    CURRENT_PROC->heap_size += size;

    return (uintptr_t)CURRENT_PROC->heap + CURRENT_PROC->heap_size;
  }
  
  CURRENT_PROC->heap_size += size;

  return (uintptr_t)CURRENT_PROC->heap + CURRENT_PROC->heap_size;
}

uint64_t c_syscall_handler(uint64_t rsp) {
  vmm_load_pagemap(&kernel_pagemap);

  registers_t *registers = (registers_t *)rsp;

  uint64_t ret = registers->rax;

  switch (registers->rax) {
    case SYSCALL_OPEN:
      ret = syscall_open((char *)registers->rdi);
      break;
    case SYSCALL_CLOSE:
      syscall_close((size_t)registers->rdi);
    case SYSCALL_READ:
      syscall_read(registers->rdi, (uint8_t *)registers->rsi, registers->rdx);
      break;
    case SYSCALL_WRITE:
      syscall_write(registers->rdi, (uint8_t *)registers->rsi, registers->rdx);
      break;
    case SYSCALL_SEEK:
      syscall_seek(registers->rdi, registers->rsi, (int)registers->rdx);
      break;
    case SYSCALL_EXEC:
      syscall_exec((char *)registers->rdi, (char *)registers->rsi,
                   registers->rdx, registers->rcx, registers->r8);
      break;
    case SYSCALL_SBRK:
      syscall_sbrk(registers->rdi);
      break;
    default:
      break;
  }

  vmm_load_pagemap(CURRENT_PAGEMAP);

  return ret;
}
