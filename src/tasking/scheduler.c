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

thread_t *threads;
proc_t *processes;

void k_idle() {
  while (1)
    ;
}

extern void switch_thread(registers_t *regs);

size_t get_next_thread(size_t offset) {
  /* MAKE_LOCK(get_next_thread_lock); */
  while (1) {
    if (offset < thread_count - 1)
      offset++;
    else
      offset = 0;

    thread_t next_thread = threads[offset];

    if (LOCKED_READ(next_thread.state) == ALIVE &&
        !LOCKED_READ(next_thread.running)) {
      threads[offset].running = 1;
      /* UNLOCK(get_next_thread_lock); */
      return offset;
    }
  }
}

void schedule(uint64_t rsp) {
  /* MAKE_LOCK(sched_lock); */

  cpu_locals_t *locals = get_locals();

  size_t index = get_next_thread(locals->last_run_thread);

  /* if (index == locals->last_run_thread) { */
  /* lapic_eoi(); */
  /* lapic_timer_oneshot(SCHEDULE_REG, threads[index].priority); */
  /* return; */
  /* } */

  if (threads[locals->last_run_thread].run_once) 
    memcpy(&threads[locals->last_run_thread].registers, (registers_t *)rsp, sizeof(registers_t));
  else
    threads[locals->last_run_thread].run_once = 1;

  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  threads[locals->last_run_thread].pagemap = (uint64_t *)cr3;

  threads[locals->last_run_thread].running = 0;

  if (cr3 != (uint64_t)threads[index].pagemap)
    asm volatile("mov %0, %%cr3" : : "r"(threads[index].pagemap));

  locals->last_run_thread = index;

  lapic_eoi();
  lapic_timer_oneshot(SCHEDULE_REG, threads[index].priority);

  /* UNLOCK(sched_lock); */

  switch_thread(&threads[index].registers);
}

void await() {
  while (LOCKED_READ(sched_started) == 0)
    ;

  asm volatile("cli");

  lapic_timer_oneshot(SCHEDULE_REG, 20);

  asm volatile("1:\n"
               "sti\n"
               "hlt\n"
               "jmp 1b\n");
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
  processes = kcalloc(sizeof(proc_t));
  threads = kcalloc(sizeof(thread_t) * 4);

  processes[0] = (proc_t){
      .name = "kproc",
      .thread_count = 1,
      .threads = kmalloc(sizeof(thread_t) * 2),
      .pid = current_pid++,
  };

  threads[0] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_init",
      .run_once = 0,
      .running = 0,
      .mother_proc = processes,
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

  threads[1] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .running = 0,
      .mother_proc = processes,
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

  threads[2] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .running = 0,
      .mother_proc = processes,
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

  threads[3] = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = "k_test",
      .run_once = 0,
      .running = 0,
      .mother_proc = processes,
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
  processes->threads[0] = threads[0];

  thread_count += 4;

  irq_install_handler(SCHEDULE_REG - 32, schedule);

  sched_started = 1;

  await();
  while (1)
    ;
}
