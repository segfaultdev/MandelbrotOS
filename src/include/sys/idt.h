#ifndef __IDT_H__
#define __IDT_H__

#include <stdint.h>

typedef struct idt_entry {
  uint16_t base_low;
  uint16_t sel;
  uint8_t ist;
  uint8_t flags;
  uint16_t base_mid;
  uint32_t base_high;
  uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed)) idt_ptr_t;

extern idt_entry_t idt[256];

void idt_set_gate(idt_entry_t *entry, int user_space, int exception,
                  void (*func)());
void load_idt();
int init_idt();

#endif
