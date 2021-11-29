#ifndef __KLOG_H__
#define __KLOG_H__

#include <stdarg.h>

void klog_init(int type, char *message);
void klog(int type, char *message, ...);

#endif
