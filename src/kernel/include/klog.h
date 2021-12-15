#ifndef __KLOG_H__
#define __KLOG_H__

#include <stdarg.h>

int klog_init(int type, char *message);
void klog(int type, char *message, ...);

#endif
