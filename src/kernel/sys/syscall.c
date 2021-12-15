#include <cpu_locals.h>
#include <drivers/ahci.h>
#include <drivers/apic.h>
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

#define CURRENT_PAGEMAP get_locals()->current_thread->mother_proc->pagemap

extern void syscall_isr();

vec_t(syscall_file_t) open_files;

lock_t syscall_lock = {0};

int init_syscalls() {
  idt_set_gate(&idt[0x45], 1, 0, syscall_isr);
  open_files.data = kmalloc(sizeof(open_files));
  return 0;
}

void syscall_print(char *string) {
  uintptr_t kern_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)string);
  puts((char *)kern_addr);
}

void syscall_putchar(uint8_t ch) { putchar(ch); }

void syscall_putpixel(size_t x, size_t y, uint32_t colour) {
  fb_putpixel(x, y, colour);
}

void syscall_open(char *path, size_t *number) {
  uintptr_t kern_path_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)path);

  fs_file_t file = vfs_get_info((char *)kern_path_addr);
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

  size_t len = (size_t)open_files.length - 1;
  vmm_memcpy(CURRENT_PAGEMAP, (uintptr_t)number, &kernel_pagemap,
             (uintptr_t)(&len), sizeof(size_t));
}

void syscall_read(size_t id, uint8_t *buffer, size_t size) {
  if (id > (size_t)open_files.length - 1)
    return;

  syscall_file_t *file = &open_files.data[id];

  if (size > file->size - file->offset)
    size = file->size - file->offset;

  vmm_memcpy(CURRENT_PAGEMAP, (uintptr_t)buffer, &kernel_pagemap,
             (uintptr_t)(file->buffer + file->offset), file->size);

  file->offset += size;
}

void c_syscall_handler(uint64_t rsp) {
  vmm_load_pagemap(&kernel_pagemap);

  registers_t *registers = (registers_t *)rsp;

  switch (registers->rax) {
    case SYSCALL_PRINT:
      syscall_print((char *)registers->rbx);
      break;
    case SYSCALL_PUTCHAR:
      syscall_putchar((uint8_t)registers->rbx);
      break;
    case SYSCALL_PUTPIXEL:
      syscall_putpixel((size_t)registers->rbx, (size_t)registers->rcx,
                       (uint32_t)registers->rdx);
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

  vmm_load_pagemap(CURRENT_PAGEMAP);
}
