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

int floor(double x) {
  int xi = (int)x;
  return x < xi ? xi - 1 : xi;
}

pixel_t hsv2rgb(float h, float s, float v) {
  int i;
  float f, p, q, t;
  if (s == 0) {
    // achromatic (grey)
    return (pixel_t){
        .red = s,
        .green = s,
        .blue = s,
    };
  }
  h /= 60; // sector 0 to 5
  i = floor(h);
  f = h - i; // factorial part of h
  p = v * (1 - s);
  q = v * (1 - s * f);
  t = v * (1 - s * (1 - f));
  switch (i) {
    case 0:
      return (pixel_t){
          .red = v * 255,
          .green = t * 255,
          .blue = p * 255,
      };
      break;
    case 1:
      return (pixel_t){
          .red = q * 255,
          .green = v * 255,
          .blue = p * 255,
      };
      break;
    case 2:
      return (pixel_t){
          .red = p * 255,
          .green = v * 255,
          .blue = t * 255,
      };
      break;
    case 3:
      return (pixel_t){
          .red = p * 255,
          .green = q * 255,
          .blue = v * 255,
      };
      break;
    case 4:
      return (pixel_t){
          .red = t * 255,
          .green = p * 255,
          .blue = v * 255,
      };
      break;
    default: // case 5:
      return (pixel_t){
          .red = v * 255,
          .green = p * 255,
          .blue = q * 255,
      };
      break;
  }
}

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

  double p = 0;

  while (1) {
    for (size_t x = 0; x < width; x++) {
      for (size_t y = 0; y < height; y++) {
        size_t raw_pos = x + y * width;
        fb_pix[raw_pos] = hsv2rgb(p, 1 - ((double)y / (double)height),
                                  (double)x / (double)width);
      }
      p += 0.0005;
      if (p > 360.0)
        p = 0;
    }
  }
}
