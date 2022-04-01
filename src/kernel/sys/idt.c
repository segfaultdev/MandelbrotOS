#include <stdint.h>
#include <sys/gdt.h>
#include <sys/idt.h>

static idt_ptr_t idtp;
idt_entry_t idt[256];

void idt_set_gate(idt_entry_t *entry, int user_space, int exception,
                  void (*func)()) {
  entry->base_low = (uint16_t)((uint64_t)(func));
  entry->base_mid = (uint16_t)((uint64_t)(func) >> 16);
  entry->base_high = (uint32_t)((uint64_t)(func) >> 32);
  entry->sel = GDT_SEG_KCODE;

  /* if (exception) */
    /* entry->flags = */
        /* (user_space) ? 0xef : 0x8f; // Present, trap and DPL 3 or 0 respectively */
  /* else */
    /* entry->flags = (user_space) */
                       /* ? 0xee */
                       /* : 0x8e; // Present, interrupt and DPL 3 or 0 respectively */
  entry->flags = 0xee;
  (void)user_space;
  (void)exception;
}

void load_idt() { asm volatile("lidt %0" : : "m"(idtp)); }

int init_idt() {
  asm volatile("cli");

  idtp.limit = sizeof(idt) - 1;
  idtp.base = (uint64_t)&idt;

  load_idt();

  return 0;
}
