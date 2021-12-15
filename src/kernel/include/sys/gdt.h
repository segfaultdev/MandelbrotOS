#ifndef __GDT_H__
#define __GDT_H__

#include <stdint.h>

#define GDT_SEG_NULL (uint16_t)((0 << 3) | 0)
#define GDT_SEG_KCODE (uint16_t)((1 << 3) | 0)
#define GDT_SEG_KDATA (uint16_t)((2 << 3) | 0)
#define GDT_SEG_UCODE (uint16_t)((3 << 3) | 3)
#define GDT_SEG_UDATA (uint16_t)((4 << 3) | 3)
#define GDT_SEG_TSS (uint16_t)((5 << 3) | 0)

typedef struct gdt_pointer {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed)) gdt_pointer_t;

typedef struct gdt_entry {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t flags;
  uint8_t limit_high : 4;
  uint8_t granularity : 4;
  uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct tss_entry {
  uint16_t length;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t flags1;
  uint8_t flags2;
  uint8_t base_high;
  uint32_t base_upper;
  uint32_t reserved;
} __attribute__((packed)) tss_entry_t;

typedef struct tss {
  uint32_t reserved0;
  uint64_t rsp[3];
  uint64_t reserved1;
  uint64_t ist[7];
  uint32_t reserved2;
  uint32_t reserved3;
  uint16_t reserved4;
  uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

typedef struct gdt {
  gdt_entry_t gdt[5];
  tss_entry_t tss;
} __attribute__((packed)) gdt_t;

void load_gdt();
void set_and_load_tss(uintptr_t addr);
int init_gdt();

#endif
