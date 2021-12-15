#include <fb/fb.h>
#include <klog.h>
#include <lock.h>
#include <printf.h>
#include <stdarg.h>
#include <stdint.h>
#include <vprintf.h>

#define KLOG_GREEN 0x55FF55
#define KLOG_RED 0xFF5555
#define KLOG_YELLOW 0xFFFF55
#define KLOG_BLUE 0x5555FF

void klog(int type, char *message, ...) {
  va_list list;
  uint32_t old_fg = curr_fg_col;

  switch (type) {
    case 0:;
      puts("[ ");
      fb_set_fg(KLOG_GREEN);
      puts("OKAY");
      fb_set_fg(old_fg);
      puts(" ] ");
      va_start(list, message);
      vprintf(message, list);
      break;
    case 1:;
      puts("[ ");
      fb_set_fg(KLOG_RED);
      puts("FAIL");
      fb_set_fg(old_fg);
      puts(" ] ");
      va_start(list, message);
      vprintf(message, list);
      break;
    case 2:;
      puts("[ ");
      fb_set_fg(KLOG_YELLOW);
      puts("WARN");
      fb_set_fg(old_fg);
      puts(" ] ");
      va_start(list, message);
      vprintf(message, list);
      break;
    case 3:;
      puts("[ ");
      fb_set_fg(KLOG_BLUE);
      puts("INFO");
      fb_set_fg(old_fg);
      puts(" ] ");
      va_start(list, message);
      vprintf(message, list);
      break;
  }
  va_end(list);
}

int klog_init(int type, char *message) {
  uint32_t old_fg = curr_fg_col;

  switch (type) {
    case 0:;
      puts("[ ");
      fb_set_fg(KLOG_GREEN);
      puts("OKAY");
      fb_set_fg(old_fg);
      printf(" ] %s initialized \r\n", message);
      break;
    case 1:;
      puts("[ ");
      fb_set_fg(KLOG_RED);
      puts("FAIL");
      fb_set_fg(old_fg);
      printf(" ] %s not initialized \r\n", message);
      break;
  }

  return type;
}
