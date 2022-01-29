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
#define SYSCALL_SBRK 6

int init_syscalls();

#endif
