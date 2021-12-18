#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <fs/vfs.h>
#include <stdint.h>

#define SYSCALL_OPEN 0
#define SYSCALL_CLOSE 1
#define SYSCALL_READ 2
#define SYSCALL_WRITE 3
#define SYSCALL_SEEK 4
#define SYSCALL_EXEC 5
#define SYSCALL_PSBRK 6

#define SEEK_ZERO 0
#define SEEK_FORWARDS 1
#define SEEK_BACKWARDS 2

typedef struct syscall_file {
  char *path;

  fs_file_t file;

  uint8_t *buffer;

  size_t index;

  size_t size;
  size_t offset;
} syscall_file_t;

int init_syscalls();

#endif
