#include <acpi/acpi.h>
#include <asm.h>
#include <boot/stivale2.h>
#include <cpu_locals.h>
#include <drivers/apic.h>
#include <drivers/pcspkr.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <lock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <printf.h>
#include <registers.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/gdt.h>
#include <sys/irq.h>
#include <tasking/scheduler.h>

#define ALIVE 0
#define DEAD 1

int sched_started = 0;

size_t thread_count = 0;
size_t proc_count = 0;

size_t current_tid = 0;
size_t current_pid = 0;

thread_t **threads;
proc_t **processes;

lock_t sched_lock = {0};

void k_idle() {
  while (1)
    ;
}

extern void switch_and_run_stack(uintptr_t stack);

size_t get_next_thread(size_t offset) {
  while (1) {
    if (offset < thread_count - 1)
      offset++;
    else
      offset = 0;

    thread_t *current_thread = LOCKED_READ(threads[offset]);

    if (LOCK_ACQUIRE(current_thread->lock)) {
      return offset;
    }
  }
}

/* thread_t *get_next_thread(size_t *index) { */
  /* size_t i = *index + 1; */
  /* while (1) { */
    /* if (i == thread_count) */
      /* i = 0; */

    /* thread_t *thread = LOCKED_READ(threads[i]); */
    /* if (LOCK_ACQUIRE(thread->lock)) { */
      /* *index = i; */
      /* return thread; */
    /* } */

    /* i++; */
  /* } */
/* } */

void schedule(uint64_t rsp) {
  cpu_locals_t *locals = get_locals();

  /* if (!LOCK_ACQUIRE(sched_lock)) */
    /* await(); */

  thread_t *current_thread = threads[locals->last_run_thread_index];

  /* if (!current_thread) { */
    /* locals->current_thread = get_next_thread(&locals->last_run_thread_index); */
    /* current_thread = locals->current_thread; */
  /* } */

  if (current_thread->run_once)
    current_thread->registers = *((registers_t *)rsp);
  else
    current_thread->run_once = 1;

  UNLOCK(current_thread->lock);

  locals->last_run_thread_index = get_next_thread(locals->last_run_thread_index);
  current_thread = threads[locals->last_run_thread_index];

  /* locals->current_thread = get_next_thread(&locals->last_run_thread_index); */
  /* current_thread = locals->current_thread; */

  /* uint64_t cr3; */
  /* asm volatile("mov %%cr3, %0" : "=r"(cr3)); */
  /* current_thread->pagemap = (uint64_t *)cr3; */

  /* asm volatile("mov %0, %%cr3" : : "r"(next_thread->pagemap)); */

  /* UNLOCK(sched_lock); */

  lapic_eoi();
  lapic_timer_oneshot(SCHEDULE_REG, current_thread->priority);

  switch_and_run_stack((uintptr_t)&current_thread->registers);
}

void await() {
  while (!LOCKED_READ(sched_started))
    ;

  lapic_timer_oneshot(SCHEDULE_REG, 20);

  asm volatile("sti");

  while (1)
    asm volatile("hlt");
}

void test() {
  while (1) {
    char x[6];
    x[0] = '2';
    x[1] = ':';
    x[2] = ' ';
    x[3] = get_locals()->cpu_number + '0';
    x[4] = '\n';
    x[5] = 0;
    serial_print(x);
    /* pcspkr_tone_on(1000); */
    for (volatile size_t i = 0; i < 5000000; i++)
      asm volatile("nop");
    /* pcspkr_tone_off(); */
  }
}

void scheduler_init(struct stivale2_struct_tag_smp *smp_info, uintptr_t addr) {
  processes = kcalloc(sizeof(proc_t *));
  threads = kcalloc(sizeof(thread_t *) * 6);

  processes[0] = kmalloc(sizeof(proc_t));
  *processes[0] = (proc_t){
      .name = "kproc",
      .thread_count = 1,
      .threads = kmalloc(sizeof(thread_t *) * 6),
      .pid = current_pid++,
  };

  threads[0] = kmalloc(sizeof(thread_t));
  *threads[0] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_init",
      .run_once = 0,
      .mother_proc = processes[0],
      .registers =
          (registers_t){
              .cs = 0x08,
              .ss = 0x10,
              .rip = (uint64_t)addr,
              .rflags = 0x202,
              .rsp = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET,
          },
      .priority = 100,
      .pagemap = get_kernel_pagemap(),
  };

  threads[1] = kmalloc(sizeof(thread_t));
  *threads[1] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .mother_proc = processes[0],
      .registers =
          (registers_t){
              .cs = 0x08,
              .ss = 0x10,
              .rip = (uint64_t)test,
              .rflags = 0x202,
              .rsp = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET,
          },
      .priority = 100,
      .pagemap = get_kernel_pagemap(),
  };

  threads[2] = kmalloc(sizeof(thread_t));
  *threads[2] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .mother_proc = processes[0],
      .registers =
          (registers_t){
              .cs = 0x08,
              .ss = 0x10,
              .rip = (uint64_t)k_idle,
              .rflags = 0x202,
              .rsp = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET,
          },
      .priority = 100,
      .pagemap = get_kernel_pagemap(),
  };

  threads[3] = kmalloc(sizeof(thread_t));
  *threads[3] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .mother_proc = processes[0],
      .registers =
          (registers_t){
              .cs = 0x08,
              .ss = 0x10,
              .rip = (uint64_t)k_idle,
              .rflags = 0x202,
              .rsp = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET,
          },
      .priority = 100,
      .pagemap = get_kernel_pagemap(),
  };

  threads[4] = kmalloc(sizeof(thread_t));
  *threads[4] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .mother_proc = processes[0],
      .registers =
          (registers_t){
              .cs = 0x08,
              .ss = 0x10,
              .rip = (uint64_t)k_idle,
              .rflags = 0x202,
              .rsp = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET,
          },
      .priority = 100,
      .pagemap = get_kernel_pagemap(),
  };

  threads[5] = kmalloc(sizeof(thread_t));
  *threads[5] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .mother_proc = processes[0],
      .registers =
          (registers_t){
              .cs = 0x08,
              .ss = 0x10,
              .rip = (uint64_t)k_idle,
              .rflags = 0x202,
              .rsp = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET,
          },
      .priority = 100,
      .pagemap = get_kernel_pagemap(),
  };

  proc_count++;
  for (size_t i = 0; i < 6; i++)
    processes[0]->threads[i] = threads[i];

  thread_count += 6;

  sched_started = 1;

  await();
}
