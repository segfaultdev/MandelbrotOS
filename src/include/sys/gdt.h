#ifndef __GDT_H__
#define __GDT_H__

#include <stdint.h>

#define GDT_SEG_NULL (0 << 3) | 0
#define GDT_SEG_KCODE (1 << 3) | 0
#define GDT_SEG_KDATA (2 << 3) | 0
#define GDT_SEG_UCODE (3 << 3) | 3
#define GDT_SEG_UDATA (4 << 3) | 3

typedef struct gdt_pointer {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed)) gdt_pointer_t;

typedef struct gdt_entry {
  uint16_t limit;
  uint16_t low;
  uint8_t mid;
  uint8_t access;
  uint8_t granularity;
  uint8_t high;
} __attribute__((packed)) gdt_entry_t;

void load_gdt();

int init_gdt();

#endif
