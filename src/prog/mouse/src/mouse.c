#include <sys/mandelbrot.h>
#include <stddef.h>
#include <stdint.h>

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
