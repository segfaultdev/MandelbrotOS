#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <fs/vfs.h>
#include <stdint.h>

#define SYSCALL_PRINT 0
#define SYSCALL_PUTCHAR 1
#define SYSCALL_PUTPIXEL 2
#define SYSCALL_OPEN 3
#define SYSCALL_READ 4

typedef struct syscall_file {
  fs_file_t file;

  uint8_t *buffer;

  size_t index;

  size_t size;
  size_t offset;
} syscall_file_t;

void syscall(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3);
int init_syscalls();

#endif
