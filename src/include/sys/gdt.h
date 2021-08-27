#ifndef __GDT_H__
#define __GDT_H__

#include <stdint.h>

#define GDT_SEG_NULL (0 << 3) | 0
#define GDT_SEG_KCODE (1 << 3) | 0
#define GDT_SEG_KDATA (2 << 3) | 0
#define GDT_SEG_UCODE (3 << 3) | 3
#define GDT_SEG_UDATA (4 << 3) | 3
#define GDT_SEG_TSS (5 << 3) | 0

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

typedef struct tss_entry {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iobp;
} __attribute__((packed)) tss_entry_t;

void load_tss(uintptr_t addr);
void load_gdt();

int init_gdt();

#endif
