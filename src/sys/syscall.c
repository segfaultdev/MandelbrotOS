#include <cpu_locals.h>
#include <drivers/ahci.h>
#include <fb/fb.h>
#include <fs/vfs.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <printf.h>
#include <registers.h>
#include <stdint.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <sys/syscall.h>

#define SAVE_PAGEMAP()                                                         \
  uint64_t current_pagemap;                                                    \
  asm volatile("mov %%cr3, %0" : "=a"(current_pagemap));                       \
  vmm_load_pagemap(&kernel_pagemap)

#define LOAD_PAGEMAP() asm volatile("mov %0, %%cr3" : : "a"(current_pagemap))

extern void syscall_isr();

vec_t(syscall_file_t) open_files;

int init_syscalls() {
  idt_set_gate(&idt[0x45], 1, 1, syscall_isr);
  open_files.data = kmalloc(sizeof(open_files));
  return 0;
}

void syscall(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
  asm volatile("mov %0, %%rax\n"
               "mov %1, %%rbx\n"
               "mov %2, %%rcx\n"
               "mov %3, %%rdx\n"
               :
               : "r"(id), "r"(arg1), "r"(arg2), "r"(arg3)
               : "memory");
  asm volatile("int $0x45");
}

void syscall_print(char *string) {
  SAVE_PAGEMAP();

  uintptr_t kern_addr = vmm_get_kernel_address(
      get_locals()->current_thread->mother_proc->pagemap, (uintptr_t)string);
  puts((char *)kern_addr);

  LOAD_PAGEMAP();
}

void syscall_putchar(uint8_t ch) {
  SAVE_PAGEMAP();
  putchar(ch);
  LOAD_PAGEMAP();
}

void syscall_open(char *path, size_t *number) {
  SAVE_PAGEMAP();

  fs_file_t file = vfs_get_info(path);
  if (!file.is_valid)
    return;

  syscall_file_t sfile = (syscall_file_t){
      .size = file.file_size,
      .offset = 0,
      .buffer = kmalloc(file.file_size),
      .file = file,
      .index = open_files.length,
  };

  vfs_read(path, 0, file.file_size, sfile.buffer);

  vec_push(&open_files, sfile);

  LOAD_PAGEMAP();

  *number = (size_t)open_files.length - 1;
}

void syscall_read(size_t id, uint8_t *buffer, size_t size) {
  SAVE_PAGEMAP();

  if (id > (size_t)open_files.length - 1)
    return;

  syscall_file_t *file = &open_files.data[id];

  if (size > file->size - file->offset)
    size = file->size - file->offset;

  vmm_memcpy(get_locals()->current_thread->mother_proc->pagemap,
             (uintptr_t)buffer, &kernel_pagemap,
             (uintptr_t)(file->buffer + file->offset),
             file->size);

  file->offset += size;

  LOAD_PAGEMAP();
}

void c_syscall_handler(uint64_t rsp) {
  registers_t *registers = (registers_t *)rsp;

  switch (registers->rax) {
    case SYSCALL_PRINT:
      syscall_print((char *)registers->rbx);
      break;
    case SYSCALL_PUTCHAR:
      syscall_putchar((uint8_t)registers->rbx);
      break;
    case SYSCALL_OPEN:
      syscall_open((char *)registers->rbx, (size_t *)registers->rcx);
      break;
    case SYSCALL_READ:
      syscall_read(registers->rbx, (uint8_t *)registers->rcx, registers->rdx);
      break;
    default:
      break;
  }
}
