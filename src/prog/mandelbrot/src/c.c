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

  if (s > 1)
    s = 1;
  if (v > 1)
    v = 1;

  if (s == 0) {
    // achromatic (grey)
    return (pixel_t){
        .red = v * 255,
        .green = v * 255,
        .blue = v * 255,
    };
  }

  while (h >= 360)
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

uint32_t rgb2hex(pixel_t pix) {
  return (pix.red << 16) + (pix.green << 8) + pix.blue;
}

void func3() { asm volatile("sti"); }
void func2() { func3(); }
void func1() { func2(); }

void main() {
  /* intsyscall(SYSCALL_WRITE, 1, (uint64_t)&"Enter", 0, 5, 0); */
  /* while (1) */
  /* ; */

  /* char* argv[2] = {0}; */
  /* char *env[2] = {0}; */
  /* argv[0] = "a"; */
  /* intsyscall(SYSCALL_EXEC, (uint64_t)"/prog/test", (uint64_t)argv,
   * (uint64_t)env, 0, 0); */

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

  /* pixel_t *pix_buf = (void *)framebuffer; */

#define ORIG_SENS 0.02
#define ORIG_ZOOM 4
#define COLOUR_DEPTH 100

  int max = 200;
  int threshold = 4;

  double posx = 0;
  double posy = 0;
  double zoom = ORIG_ZOOM;
  double sensitivity = ORIG_SENS;

  uint32_t colours[COLOUR_DEPTH];
  for (size_t i = 0; i < (size_t)COLOUR_DEPTH; i++)
    /* colours[i] = */
    /* rgb2hex(hsv2rgb(((double)i / (double)COLOUR_DEPTH) * (360 - 265) + 265,
     */
    /* 1, (double)i / (double)(i + 5))); */
    colours[i] =
        rgb2hex(hsv2rgb(((double)i / (double)COLOUR_DEPTH) * (360 - 300) + 0, 1,
                        (double)i / (double)(i + 5)));
  /* colours[i] = rgb2hex(hsv2rgb(0, 0, (double)i / (double)COLOUR_DEPTH +
   * 0.1)); */
  /* colours[i] = */
  /* rgb2hex(hsv2rgb(((double)i / (double)COLOUR_DEPTH) * (360 - 10) + 10, 1, */
  /* ((double)i / (double)(COLOUR_DEPTH / 16)))); */

  while (1) {
    char c;
    intsyscall(SYSCALL_READ, 1, (uint64_t)&c, 0, 1, 0);

    switch (c) {
      case 'w':
        posy -= sensitivity;
        break;
      case 's':
        posy += sensitivity;
        break;
      case 'a':
        posx -= sensitivity;
        break;
      case 'd':
        posx += sensitivity;
        break;
      case 'z':
        zoom -= sensitivity;
        break;
      case 'x':
        zoom += sensitivity;
        break;
      case 'r':
        zoom = ORIG_ZOOM;
        sensitivity = ORIG_SENS;
        break;
      case 'c':
        posy = 0;
        posx = 0;
        break;
      default:
        continue;
    }

    sensitivity = zoom / 10;

    for (int row = 0; row < height; row++) {
      for (int col = 0; col < width; col++) {
        double c_re = (col - width / 2.0) * zoom / width + posx;
        double c_im = (row - height / 2.0) * zoom / width + posy;
        double x = 0, y = 0;
        int iteration = 0;
        /* threshold = zoom; */
        /* max = ABS((1 / zoom * 10) + 50); */
        /* max = 10 / zoom; */

        /* while (x * x + y * y <= threshold && iteration < max) { */
        /* double x_new = x * x - y * y + c_re; */
        /* y = 2 * x * y + c_im; */
        /* x = x_new; */
        /* iteration++; */
        /* } */

        while (x * x + y * y <= threshold && iteration < max) {
          double x_new = x * x - y * y + c_re;
          y = 2 * ABS(x * y) + c_im;
          x = x_new;
          iteration++;
        }

        if (iteration < max)
          /* pix_buf[col + row * width] = */
          /* hsv2rgb(((double)row / (double)iteration), 1, 1); */
          /* hsv2rgb(((double)col / (double)) * 360, 1, 1); */
          /* (pixel_t){0, 0, 0, 0}; */
          framebuffer[col + row * width] = colours[(
              size_t)(((double)iteration / (double)max) * COLOUR_DEPTH)];
        else
          framebuffer[col + row * width] = 0;
        /* framebuffer[col + row * width] = rgb2hex(hsv2rgb(((double)row / */
        /* (double)height) * 360, 1, 1));  */
        /* pix_buf[col + row * width] =  */
        /* hsv2rgb(((double)col / (double)row) * 360, 1, 1); */

      }
    }
        for (size_t i = 0; i < 21; i++)
          framebuffer[height / 2 * width + (width / 2 - 10 + i)] ^= 0xffffff;
        for (size_t i = 0; i < 21; i++)
          framebuffer[(height / 2 - 10 + i) * width + width / 2] ^= 0xffffff;
  }
}
