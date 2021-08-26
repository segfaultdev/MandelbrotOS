#include <lock.h>
#include <stdint.h>
#include <sys/gdt.h>

static gdt_entry_t gdt[7]; // 5 entries and 2 for the TSS
static gdt_pointer_t gdt_ptr;

__attribute__((optimize((3)))) void load_gdt() {
  asm volatile("lgdt %0" : : "m"(gdt_ptr) : "memory");

  asm volatile("mov %%rsp, %%rax\n"
               "push %0\n"
               "push %%rax\n"
               "pushf\n"
               "push %1\n"
               "push $reload_segments\n"
               "iretq\n"
               "reload_segments:\n"
               "mov %0, %%ax\n"
               "mov %%ax, %%ds\n"
               "mov %%ax, %%es\n"
               "mov %%ax, %%ss\n"
               "mov %2, %%ax\n"
               "mov %%ax, %%fs\n"
               "mov %%ax, %%gs\n"
               "mov %0, %%ax\n"
               :
               : "rmi"(GDT_SEG_KDATA), "rmi"(GDT_SEG_KCODE),
                 "rmi"(GDT_SEG_UDATA)
               : "rax", "memory");
}

void load_tss(uintptr_t addr) {
  MAKE_LOCK(tss_lock);

  gdt[5] = (gdt_entry_t){
      .limit = 103,
      .low = (uint16_t)((uint64_t)addr),
      .mid = (uint8_t)((uint64_t)addr >> 16),
      .high = (uint8_t)((uint64_t)addr >> 24),
      .access = 0b10001001,
      .granularity = 0,
  };

  gdt[6] = (gdt_entry_t){
      .limit = (uint16_t)((uint64_t)addr >> 32),
      .low = (uint16_t)((uint64_t)addr >> 48),
  };

  asm volatile("ltr %%ax\n" : : "a"(GDT_SEG_TSS) : "memory");

  UNLOCK(tss_lock);
}

int init_gdt() {
  gdt_ptr.limit = sizeof(gdt) - 1;
  gdt_ptr.base = (uint64_t)gdt;

  gdt[0] = (gdt_entry_t){.low = 0x0,
                         .mid = 0x0,
                         .high = 0x0,
                         .limit = 0x0,
                         .access = 0x0,
                         .granularity = 0x0}; // NULL descriptor

  gdt[1] = (gdt_entry_t){.low = 0x0,
                         .mid = 0x0,
                         .high = 0x0,
                         .limit = 0x0,
                         .access = 0x9a,
                         .granularity = 0x20}; // Kernel code descriptor

  gdt[2] = (gdt_entry_t){.low = 0x0,
                         .mid = 0x0,
                         .high = 0x0,
                         .limit = 0x0,
                         .access = 0x92,
                         .granularity = 0x0}; // Kernel data descriptor

  gdt[3] = (gdt_entry_t){.low = 0x0,
                         .mid = 0x0,
                         .high = 0x0,
                         .limit = 0x0,
                         .access = 0xfa,
                         .granularity = 0x20}; // User code descriptor

  gdt[4] = (gdt_entry_t){.low = 0x0,
                         .mid = 0x0,
                         .high = 0x0,
                         .limit = 0x0,
                         .access = 0xf2,
                         .granularity = 0x0}; // User data descriptor

  load_gdt();

  return 0;
}
