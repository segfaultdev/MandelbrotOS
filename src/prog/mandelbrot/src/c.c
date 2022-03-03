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

#define ITTERATIONS 80

#define ABS(x) ((x < 0) ? (-x) : x)

uint16_t width;
uint16_t height;
uint32_t *framebuffer;

uint64_t intsyscall(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                    uint64_t arg4, uint64_t arg5);

static inline double cos(double x) {
  register double val;
  asm volatile("fcos\n" : "=t"(val) : "0"(x));
  return val;
}

static inline double sin(double x) {
  register double val;
  asm volatile("fsin\n" : "=t"(val) : "0"(x));
  return val;
}

static inline double tan(double x) { return sin(x) / cos(x); }

static inline double sqrt(double x) {
  double res;
  asm("fsqrt" : "=t"(res) : "0"(x));
  return res;
}

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

  while (h > 360)
    h -= 360;

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

  pixel_t *pix_buf = (void *)framebuffer;

  int max = 100;

  for (size_t i = 0; i < 2000000; i += 10) {
    for (int row = 0; row < height; row++) {
      for (int col = 0; col < width; col++) {
        double c_re = (col - width / 2.0) * 4.0 / width;
        double c_im = (row - height / 2.0) * 4.0 / width;
        double x = 0, y = 0;
        int iteration = 0;
        while (x * x + y * y <= 4 && iteration < max) {
          double x_new = x * x - y * y + c_re;
          y = 2 * x * y + c_im;
          x = x_new;
          iteration++;
        }
        if (iteration < max)
          pix_buf[col + row * width] =
              hsv2rgb(((double)iteration / (double)max) * i, 1, 1);
        /* hsv2rgb(((double)col / (double)) * 360, 1, 1); */
        /* (pixel_t){0, 0, 0, 0}; */
        else
          framebuffer[col + row * width] = 0;
        /* pix_buf[col + row * width] =  */
        /* hsv2rgb(((double)col / (double)row) * 360, 1, 1); */
      }
    }
  }
}
