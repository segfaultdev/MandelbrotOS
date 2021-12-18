#include <stddef.h>
#include <stdint.h>

#define SYSCALL_PRINT 0
#define SYSCALL_PUTCHAR 1
#define SYSCALL_PUTPIXEL 2
#define SYSCALL_OPEN 3
#define SYSCALL_READ 4

#define ITTERATIONS 60

#define ABS(x) ((x < 0) ? (-x) : x)

uint16_t width;
uint16_t height;
uint32_t *framebuffer;

void intsyscall(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3);

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

void rect(size_t startx, size_t starty, size_t length, size_t height,
          uint32_t colour) {
  for (size_t x = startx; x < startx + length; x++)
    for (size_t y = starty; y < starty + height; y++)
      framebuffer[y * width + x] = colour;
}

uint32_t rgb2hex(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
}

void draw(double real, double imag) {
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      double cx = 2.0 * ((j / (double)(width / 2)) - 1.0);
      double cy = 2.0 * ((i / (double)(height / 2)) - 1.0);

      double zx = cx;
      double zy = cy;

      int k = 0;

      while (k < ITTERATIONS && ABS(zx) < 2 && ABS(zy) < 2) {
        double nx = zx * zx - zy * zy + real;
        double ny = 2 * zx * zy + imag;

        zx = nx;
        zy = ny;

        k++;
      }

      if (k == ITTERATIONS)
        rect(j, i, 1, 1, 0);
      else {
        double a = k / 20.0;
        double b = a - 1.0;

        if (a > 1.0)
          a = 1.0;
        if (b < 0.0)
          b = 0.0;

        double col_r = (0 * (1 - a) + 0 * a) * (1 - b) + 255 * b;
        double col_g = (7 * (1 - a) + 255 * a) * (1 - b) + 255 * b;
        double col_b = (23 * (1 - a) + 127 * a) * (1 - b) + 255 * b;

        if (col_r > 255.0)
          col_r = 255.0;
        if (col_g > 255.0)
          col_g = 255.0;
        if (col_b > 255.0)
          col_b = 255.0;

        rect(j, i, 1, 1, rgb2hex(col_r, col_g, col_b));
      }
    }
  }
}

void main(uint64_t arg1, uint64_t arg2, uint64_t arg3) {
  framebuffer = (uint32_t *)arg1;
  width = arg2;
  height = arg3;

  // 4 really cool alorithms to choose from

  double a = 0;
  while (1) {
    draw(0.7885 * cos(a), 0.7885 * sin(a));
    a += 0.005;
  }

  /* double a = 0; */
  /* while (1) { */
  /* draw(0.7885 * tan(a), 0.7885 * sin(a)); */
  /* a += 0.005; */
  /* } */

  /* double a = 0; */
  /* while (1) { */
  /* draw(tan(a), tan(sin(a * 10) * 1)); */
  /* a += 0.005; */
  /* } */

  /* double a = 0; */
  /* while (1) { */
  /* draw(tan(a), cos(a)/(1 + (1 / (1 + cos(a * a)) + ABS(tan(a * a))))); */
  /* a += 0.005; */
  /* } */
}
