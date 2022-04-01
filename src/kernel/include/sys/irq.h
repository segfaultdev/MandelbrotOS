#ifndef __IRQ_H__
#define __IRQ_H__

#include <stdint.h>

void irq_install_handler(int irq, void (*handler)(uint64_t rsp));
void irq_uninstall_handler(int irq);
int init_irq();

#endif
