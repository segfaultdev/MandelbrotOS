#ifndef __PS2_H__
#define __PS2_H__

#include <stdint.h>

uint8_t getchar();
void getmouse(int8_t *bytes);
int init_ps2();

#endif
