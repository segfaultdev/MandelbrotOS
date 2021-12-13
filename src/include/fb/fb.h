#ifndef __FB_H__
#define __FB_H__

#include <boot/stivale2.h>
#include <stddef.h>

extern uint32_t curr_fg_col;
extern uint32_t curr_bg_col;

void fb_set_bg(uint32_t bg_col);
void fb_set_fg(uint32_t fg_col);

void fb_putpixel(size_t x, size_t y, uint32_t color);
int init_fb(struct stivale2_struct_tag_framebuffer *framebuffer_info);
void putnc(int x, int y, char c, uint32_t fgc, uint32_t bgc);
void putchar(char c);
void puts(char *c);

#endif
