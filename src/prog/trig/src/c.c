#include <stddef.h>
#include <stdint.h>

#define SYSCALL_PRINT 0
#define SYSCALL_PUTCHAR 1
#define SYSCALL_PUTPIXEL 2
#define SYSCALL_OPEN 3
#define SYSCALL_READ 4

uint16_t width;
uint16_t height;
uint32_t *framebuffer;

void main(uint64_t arg1, uint64_t arg2, uint64_t arg3) {
  framebuffer = (uint32_t *)arg1;
  width = arg2;
  height = arg3;

  for (size_t i = 0; i < height * width; i++)
    framebuffer[i] = 0xffffff;

  for (int y = height - 1, row = 0; y >= 0; y--, row++) 
    for (int x = 0; x + y < height; x++) 
      framebuffer[(row * width) + (y / 2) + x] = (x & y) ? 0xffffff : 0;
}
