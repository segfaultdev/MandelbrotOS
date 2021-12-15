#include <lock.h>
#include <stdint.h>
#include <sys/gdt.h>

static volatile lock_t gdt_lock = {0};

static gdt_t gdt;
static gdt_pointer_t gdt_ptr;

void load_gdt() {
  asm volatile("lgdt %0" : : "m"(gdt_ptr) : "memory");

  asm volatile("mov %%rsp, %%rax\n"
               "push %0\n"
               "push %%rax\n"
               "pushf\n"
               "push %1\n"
               "push $reload_segments%=\n"
               "iretq\n"
               "reload_segments%=:\n"
               "mov %0, %%ax\n"
               "mov %%ax, %%ds\n"
               "mov %%ax, %%es\n"
               "mov %%ax, %%ss\n"
               "mov %%ax, %%fs\n"
               "mov %%ax, %%gs\n"
               :
               : "rmi"(GDT_SEG_KDATA), "rmi"(GDT_SEG_KCODE)
               : "rax", "memory");
}

void load_tss(uint16_t segment) {
  asm volatile("mov %0, %%ax\n"
               "ltr %%ax"
               :
               : "r"(segment)
               : "ax");
}

int init_gdt() {
  gdt_ptr.limit = sizeof(gdt) - 1;
  gdt_ptr.base = (uint64_t)&gdt;

  gdt.gdt[0] = (gdt_entry_t){0}; // NULL descriptor

  gdt.gdt[1] = (gdt_entry_t){.limit_low = 0,
                             .base_low = 0,
                             .base_mid = 0,
                             .flags = 0x9a,
                             .limit_high = 0,
                             .granularity = 0x2,
                             .base_high = 0}; // Kernel code descriptor

  gdt.gdt[2] = (gdt_entry_t){.limit_low = 0,
                             .base_low = 0,
                             .base_mid = 0,
                             .flags = 0x92,
                             .limit_high = 0,
                             .granularity = 0,
                             .base_high = 0}; // Kernel data descriptor

  gdt.gdt[3] = (gdt_entry_t){.limit_low = 0,
                             .base_low = 0,
                             .base_mid = 0,
                             .flags = 0xfa,
                             .limit_high = 0,
                             .granularity = 0x2,
                             .base_high = 0}; // User code descriptor

  gdt.gdt[4] = (gdt_entry_t){.limit_low = 0,
                             .base_low = 0,
                             .base_mid = 0,
                             .flags = 0xf2,
                             .limit_high = 0,
                             .granularity = 0x0,
                             .base_high = 0}; // User data descriptor

  load_gdt();

  return 0;
}

void set_and_load_tss(uintptr_t addr) {
  LOCK(gdt_lock);

  gdt.tss = (tss_entry_t){
      .length = sizeof(tss_entry_t),
      .base_low = addr & 0xffff,
      .base_mid = (addr >> 16) & 0xff,
      .flags1 = 0b10001001,
      .flags2 = 0,
      .base_high = (addr >> 24) & 0xff,
      .base_upper = addr >> 32,
      .reserved = 0,
  };

  load_tss(GDT_SEG_TSS);

  UNLOCK(gdt_lock);
}
