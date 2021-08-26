#ifndef __CPU_LOCALS_H__
#define __CPU_LOCALS_H__

#include <asm.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/gdt.h>

typedef struct cpu_locals {
  size_t last_run_tid;
  size_t cpu_number;
  size_t lapic_id;
  tss_entry_t tss;
} cpu_locals_t;

static inline cpu_locals_t *get_locals() {
  return (cpu_locals_t *)rdmsr(0xc0000101);
}

static inline void set_locals(cpu_locals_t *local) {
  wrmsr(0xc0000101, (uint64_t)local);
}

#endif
