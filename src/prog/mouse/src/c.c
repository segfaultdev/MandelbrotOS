#include <stddef.h>
#include <stdint.h>

#define SYSCALL_OPEN 0
#define SYSCALL_CLOSE 1
#define SYSCALL_READ 2
#define SYSCALL_WRITE 3
#define SYSCALL_EXEC 4
#define SYSCALL_MMAP 5
#define SYSCALL_MUNMAP 6
#define SYSCALL_STAT 7
#define SYSCALL_FSTAT 8
#define SYSCALL_GETPID 9
#define SYSCALL_EXIT 10
#define SYSCALL_FORK 11
#define SYSCALL_GETTIMEOFDAY 12
#define SYSCALL_FSYNC 13
#define SYSCALL_IOCTL 14

#define IOCTL_FBDEV_GET_WIDTH 1
#define IOCTL_FBDEV_GET_HEIGHT 2
#define IOCTL_FBDEV_GET_BPP 3

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#define PROT_NONE 0x0

#define MAP_PRIVATE 0x1
#define MAP_SHARED 0x2
#define MAP_FIXED 0x4
#define MAP_ANON 0x8

uint64_t intsyscall(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                    uint64_t arg4, uint64_t arg5);

void main() {
  size_t fd = intsyscall(SYSCALL_OPEN, (uint64_t) "/dev/mouse0", 0, 0, 0, 0);

  while (1) {
    uint8_t c[4] = {0};
    intsyscall(SYSCALL_READ, fd, (uint64_t)c, 0, 3, 0);

    if (c[0] & 1)
      intsyscall(SYSCALL_WRITE, 1, (uint64_t) "Left click\r\n", 0, 12, 0);
    else if (c[0] & 0b10)
      intsyscall(SYSCALL_WRITE, 1, (uint64_t) "Right click\r\n", 0, 13, 0);
    /* else */
    /* intsyscall(SYSCALL_WRITE, 1, (uint64_t)"No click\r\n", 0, 10, 0); */
    /* if (c[0]) */
    /* intsyscall(SYSCALL_WRITE, 1, (uint64_t)c, 0, 3, 0); */
    /* for (volatile size_t i = 0; i < 1000000000; i++) */
    /* asm volatile("nop"); */
  }
}
