/**
 * julia.c
 *
 * Written in 2021 by @segfaultdev
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <https://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stddef.h>
#include <stdint.h>

#define SYSCALL_PRINT 0
#define SYSCALL_PUTCHAR 1
#define SYSCALL_PUTPIXEL 2
#define SYSCALL_OPEN 3
#define SYSCALL_READ 4

#define HEIGHT 768
#define WIDTH 1024
#define ITTERATIONS 80

#define ABS(x) ((x < 0) ? (-x) : x)

void syscall(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3);

void rect(size_t startx, size_t starty, size_t length, size_t height,
          uint32_t colour) {
  for (size_t x = startx; x < startx + length; x++)
    for (size_t y = starty; y < starty + height; y++)
      syscall(SYSCALL_PUTPIXEL, (uint64_t)x, (uint64_t)y, (uint64_t)colour);
}

uint32_t rgb2hex(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
}

void draw(double real, double imag, double zoom) {
  for (int i = 0; i < HEIGHT; i++) {
    for (int j = 0; j < WIDTH; j++) {
      double cx = 2.0 * ((j / (double)(WIDTH / 2)) - 1.0);
      double cy = 2.0 * ((i / (double)(HEIGHT / 2)) - 1.0);

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
        rect(j * zoom, i * zoom, zoom, zoom, 0);
      else {
        double a = k / 20.0;
        double b = a - 1.0;

        if (a > 1.0)
          a = 1.0;
        if (b < 0.0)
          b = 0.0;

        double col_r = (0 * (1 - a) + 0 * a) * (1 - b) + 255 * b;
        if (col_r > 255.0)
          col_r = 255.0;
        double col_g = (7 * (1 - a) + 255 * a) * (1 - b) + 255 * b;
        if (col_g > 255.0)
          col_g = 255.0;
        double col_b = (23 * (1 - a) + 127 * a) * (1 - b) + 255 * b;
        if (col_b > 255.0)
          col_b = 255.0;

        rect(j * zoom, i * zoom, zoom, zoom, rgb2hex(col_r, col_g, col_b));
      }
    }
  }
}

void cmain() {
  draw(-0.8, 0.156, 1);
}
