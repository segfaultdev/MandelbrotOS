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

uint16_t width;
uint16_t height;
uint32_t *framebuffer;

typedef struct pixel {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
  uint8_t unused;
} pixel_t;

typedef struct mmap_args {
  void *addr;
  size_t length;
  uint64_t flags;
  uint64_t prot;
  size_t fd;
  size_t offset;
} mmap_args_t;

void main() {
  size_t fd = intsyscall(SYSCALL_OPEN, (uint64_t) "/dev/fb0", 0, 0, 0, 0);

  width = intsyscall(SYSCALL_IOCTL, fd, IOCTL_FBDEV_GET_WIDTH, (uint64_t)NULL,
                     0, 0);
  height = intsyscall(SYSCALL_IOCTL, fd, IOCTL_FBDEV_GET_HEIGHT, (uint64_t)NULL,
                      0, 0);
  size_t bpp = intsyscall(SYSCALL_IOCTL, fd, IOCTL_FBDEV_GET_HEIGHT,
                          (uint64_t)NULL, 0, 0);

  mmap_args_t args = (mmap_args_t){
      .addr = NULL,
      .fd = fd,
      .length = width * height * (bpp / 8),
      .offset = 0,
      .prot = PROT_READ | PROT_WRITE,
      .flags = 0,
  };

  framebuffer =
      (uint32_t *)intsyscall(SYSCALL_MMAP, (uint64_t)&args, 0, 0, 0, 0);

  pixel_t *fb_pix = (void *)framebuffer;

  size_t current_offset = 0;

  while (1) {
    for (size_t x = 0; x < width; x++) {
      for (size_t y = 0; y < height; y++) {
        size_t raw_pos = x + y * width;
        fb_pix[raw_pos].blue =
            (x * 4 + current_offset) ^ (y * 4 + current_offset);
        fb_pix[raw_pos].red =
            (y * 1 + current_offset) ^ (x * 1 + current_offset);
        fb_pix[raw_pos].green =
            (y * 2 + current_offset) ^ (x * 2 + current_offset);

        /* fb_pix[raw_pos].blue = y + current_offset; */
        /* fb_pix[raw_pos].red = x + current_offset; */
        /* fb_pix[raw_pos].green = (y & x) + current_offset; */

        /* fb_pix[raw_pos].blue = y + current_offset; */
        /* fb_pix[raw_pos].red = x + current_offset; */
        /* fb_pix[raw_pos].green = y * x + current_offset; */

        /* fb_pix[raw_pos].blue = x; */
        /* fb_pix[raw_pos].red = y; */
        /* fb_pix[raw_pos].green = current_offset; */

        /* framebuffer[raw_pos] = current_offset++; */
      }
    }

    current_offset += 1;

    /* if (current_offset < 5) { */
    /* reverse = 0; */
    /* current_offset++; */
    /* } */
    /* else if (reverse) */
    /* current_offset -= 1; */
    /* else if (current_offset < 0xff) */
    /* current_offset += 1; */
    /* else  */
    /* reverse = 1; */
  }
}
