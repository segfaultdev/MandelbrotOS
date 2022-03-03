#include <cpu_locals.h>
#include <drivers/ahci.h>
#include <drivers/apic.h>
#include <drivers/rtc.h>
#include <elf.h>
#include <fb/fb.h>
#include <fs/vfs.h>
#include <lock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <printf.h>
#include <registers.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <sys/syscall.h>
#include <tasking/scheduler.h>
#include <vec.h>

#define ALIGN_UP(__addr, __align) (((__addr) + (__align)-1) & ~((__align)-1))

#define CURRENT_PAGEMAP get_locals()->current_thread->mother_proc->pagemap
#define CURRENT_PROC get_locals()->current_thread->mother_proc

#define MAX_HEAP_SIZE 0x10000

extern void syscall_isr();

lock_t syscall_lock = {0};

int init_syscalls() {
  idt_set_gate(&idt[0x45], 1, 1, syscall_isr);
  return 0;
}

size_t syscall_open(char *path) {
  uintptr_t kern_path_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)path);

  fs_file_t *file = vfs_open((char *)kern_path_addr);

  if (!file)
    return (size_t)-1;

  if (S_ISDIR(file->mode)) {
    vfs_close(file);
    return -1;
  }

  syscall_file_t *sfile = kmalloc(sizeof(syscall_file_t));
  *sfile = (syscall_file_t){
      .file = file,
      .buf = kmalloc(file->length),
      .dirty = 0,
      .fd = CURRENT_PROC->last_fd++,
      .is_buffered = 1,
  };

  vec_push(&CURRENT_PROC->fds, sfile);

  return (size_t)sfile->fd;
}

void syscall_close(size_t id) {
  syscall_file_t *file = NULL;
  for (size_t i = 0; i < (size_t)CURRENT_PROC->fds.length; i++)
    if (CURRENT_PROC->fds.data[i]->fd == id)
      file = CURRENT_PROC->fds.data[i];
  if (!file)
    return;

  if (file->is_buffered && file->dirty)
    vfs_write(file->file, file->buf, 0, file->file->length);

  kfree(file->buf);
  vfs_close(file->file);
}

void syscall_read(size_t id, uint8_t *buffer, size_t offset, size_t size) {
  syscall_file_t *file = NULL;
  for (size_t i = 0; i < (size_t)CURRENT_PROC->fds.length; i++)
    if (CURRENT_PROC->fds.data[i]->fd == id)
      file = CURRENT_PROC->fds.data[i];
  if (!file)
    return;

  if (file->is_buffered)
    vmm_memcpy(CURRENT_PAGEMAP, (uintptr_t)buffer, &kernel_pagemap,
               (uintptr_t)(file->buf + offset), file->file->length);
  else
    vfs_read(file->file, buffer, offset, size);
}

void syscall_write(size_t id, uint8_t *buffer, size_t offset, size_t size) {
  syscall_file_t *file = NULL;
  for (size_t i = 0; i < (size_t)CURRENT_PROC->fds.length; i++)
    if (CURRENT_PROC->fds.data[i]->fd == id)
      file = CURRENT_PROC->fds.data[i];
  if (!file)
    return;

  if (file->is_buffered) {
    file->dirty = 1;

    if (size > file->file->length - offset)
      file->buf = krealloc(file->buf, size + offset);

    vmm_memcpy(
        &kernel_pagemap, (uintptr_t)(file->buf + offset), CURRENT_PAGEMAP,
        (uintptr_t)vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)buffer),
        size);
  } else
    vfs_write(
        file->file,
        (uint8_t *)vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)buffer),
        offset, size);
}

int syscall_execve(char *name, char *path, char *argv, char *env) {
  uintptr_t name_kern_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)name);
  uintptr_t path_kern_addr =
      vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)path);

  uintptr_t kargv = vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)argv);
  uintptr_t kenv = vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)env);

  proc_t *new_proc = sched_create_proc((char *)name_kern_addr, 1);

  if (elf_run_binary((char *)name_kern_addr, (char *)path_kern_addr, new_proc,
                     STANDARD_TIME_SLICE, kargv, kenv, 0))
    return 1;

  sched_destroy_proc(CURRENT_PROC);

  return 0; // Silly little GCC. Don't you understand that we never get here?
}

void *syscall_mmap(mmap_args_t *args) {
  mmap_args_t *kargs =
      (mmap_args_t *)vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)args);

  uintptr_t addr;
  size_t needed_pages = ALIGN_UP(kargs->length, PAGE_SIZE) / PAGE_SIZE;

  if (kargs->flags & MAP_FIXED) {
    if (vmm_virt_to_phys(CURRENT_PAGEMAP, (uintptr_t)kargs->addr))
      return NULL;
    addr = (uintptr_t)kargs->addr;
  } else {
    addr = CURRENT_PROC->mmap_anon_last;
    CURRENT_PROC->mmap_anon_last += ((needed_pages + 1) * PAGE_SIZE);
  }

  if (kargs->flags & MAP_ANON) {
    uintptr_t given_pages = (uintptr_t)pcalloc(needed_pages);

    for (size_t i = 0; i < needed_pages; i++)
      vmm_map_page(CURRENT_PAGEMAP, given_pages + i, addr + (i * PAGE_SIZE),
                   (kargs->prot & PROT_WRITE) ? 0b111 : 0b101);

    mmap_range_t *mmap_range = kmalloc(sizeof(mmap_range_t));
    *mmap_range = (mmap_range_t){
        .file = NULL,
        .flags = kargs->flags,
        .length = needed_pages * PAGE_SIZE,
        .offset = kargs->offset,
        .prot = kargs->prot,
        .phys_addr = given_pages,
        .virt_addr = addr,
    };

    vec_push(&CURRENT_PAGEMAP->ranges, mmap_range);
  } else {
    syscall_file_t *file = NULL;
    for (size_t i = 0; i < (size_t)CURRENT_PROC->fds.length; i++)
      if (CURRENT_PROC->fds.data[i]->fd == kargs->fd)
        file = CURRENT_PROC->fds.data[i];
    if (!file)
      return NULL;

    (void)vfs_mmap(file->file, CURRENT_PAGEMAP, file, (void *)addr,
                   kargs->length, kargs->offset, kargs->prot, kargs->flags);
  }

  return (void *)addr;
}

int syscall_munmap(void *addr, size_t length) {
  for (size_t i = 0; i < (size_t)CURRENT_PAGEMAP->ranges.length; i++) {
    if ((void *)CURRENT_PAGEMAP->ranges.data[i]->virt_addr == addr) {
      vec_remove(&CURRENT_PAGEMAP->ranges, CURRENT_PAGEMAP->ranges.data[i]);
      for (size_t i = 0; i < length; i += PAGE_SIZE)
        vmm_unmap_page(CURRENT_PAGEMAP, (uintptr_t)(addr + i));
      return 0;
    }
  }
  return 1;
}

void syscall_stat(char *path, stat_t *stat) {
  char *kpath =
      (char *)vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)path);
  stat_t *kstat =
      (stat_t *)vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)stat);

  fs_file_t *file = vfs_open(kpath);
  vfs_fstat(file, kstat);
  vfs_close(file);
}

void syscall_fstat(size_t id, stat_t *stat) {
  stat_t *kstat =
      (stat_t *)vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)stat);

  syscall_file_t *file = NULL;
  for (size_t i = 0; i < (size_t)CURRENT_PROC->fds.length; i++)
    if (CURRENT_PROC->fds.data[i]->fd == id)
      file = CURRENT_PROC->fds.data[i];
  if (!file)
    return;

  vfs_fstat(file->file, kstat);
}

size_t syscall_getpid() { return CURRENT_PROC->pid; }

size_t syscall_getppid() { return CURRENT_PROC->ppid; }

void syscall_exit(size_t code) {
  (void)code;
  sched_destroy_proc(CURRENT_PROC);
}

size_t syscall_fork(registers_t *regs) { return sched_fork(regs); }

void syscall_gettimeofday(posix_time_t *time) {
  posix_time_t *ktime =
      (posix_time_t *)vmm_get_kernel_address(CURRENT_PAGEMAP, (uintptr_t)time);

  *ktime = rtc_mktime(rtc_get_datetime());
}

int syscall_fsync(size_t id) {
  syscall_file_t *file = NULL;
  for (size_t i = 0; i < (size_t)CURRENT_PROC->fds.length; i++)
    if (CURRENT_PROC->fds.data[i]->fd == id)
      file = CURRENT_PROC->fds.data[i];
  if (!file)
    return 1;

  if (file->dirty && file->is_buffered)
    vfs_write(file->file, file->buf, 0, file->file->length);

  return 0;
}

uint64_t syscall_ioctl(size_t fd, uint64_t cmd, void *arg) {
  syscall_file_t *file = NULL;
  for (size_t i = 0; i < (size_t)CURRENT_PROC->fds.length; i++)
    if (CURRENT_PROC->fds.data[i]->fd == fd)
      file = CURRENT_PROC->fds.data[i];
  if (!file)
    return -1;

  return vfs_ioctl(file->file, cmd, arg);
}

extern void switch_and_run_stack();

uint64_t c_syscall_handler(uint64_t rsp) {
  vmm_load_pagemap(&kernel_pagemap);

  registers_t *registers = (registers_t *)rsp;

  uint64_t ret = registers->rax;

  switch (registers->rdi) {
    case SYSCALL_OPEN:
      ret = syscall_open((char *)registers->rsi);
      break;
    case SYSCALL_CLOSE:
      syscall_close((size_t)registers->rsi);
      break;
    case SYSCALL_READ:
      syscall_read(registers->rsi, (uint8_t *)registers->rdx, registers->rcx,
                   registers->r8);
      break;
    case SYSCALL_WRITE:
      syscall_write(registers->rsi, (uint8_t *)registers->rdx, registers->rcx,
                    registers->r8);
      break;
    case SYSCALL_EXEC:
      ret = syscall_execve((char *)registers->rsi, (char *)registers->rdx,
                           (char *)registers->rcx, (char *)registers->r8);
      break;
    case SYSCALL_MMAP:
      ret = (uint64_t)syscall_mmap((mmap_args_t *)registers->rsi);
      break;
    case SYSCALL_MUNMAP:
      ret = (uint64_t)syscall_munmap((void *)registers->rsi,
                                     (size_t)registers->rdx);
      break;
    case SYSCALL_STAT:
      syscall_stat((char *)registers->rsi, (stat_t *)registers->rdx);
      break;
    case SYSCALL_FSTAT:
      syscall_fstat((size_t)registers->rsi, (stat_t *)registers->rdx);
      break;
    case SYSCALL_GETPID:
      ret = syscall_getpid();
      break;
    case SYSCALL_GETPPID:
      ret = syscall_getppid();
      break;
    case SYSCALL_EXIT:
      syscall_exit(registers->rsi);
      break;
    case SYSCALL_FORK:
      ret = syscall_fork(registers);
      break;
    case SYSCALL_GETTIMEOFDAY:
      syscall_gettimeofday((posix_time_t *)registers->rsi);
      break;
    case SYSCALL_FSYNC:
      syscall_fsync((size_t)registers->rsi);
      break;
    case SYSCALL_IOCTL:
      ret = syscall_ioctl((size_t)registers->rsi, registers->rdx,
                          (void *)registers->rcx);
      break;
    default:
      ret = -1;
      break;
  }

  vmm_load_pagemap(CURRENT_PAGEMAP);

  return ret;
}
