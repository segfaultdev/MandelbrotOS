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
#include <vec.h>

#define ALIVE 0
#define DEAD 1

int sched_started = 0;

size_t thread_count = 0;
size_t proc_count = 0;

size_t current_tid = 0;
size_t current_pid = 0;

static vec_t(thread_t *) threads = {};
static vec_t(proc_t *) processes = {};

lock_t sched_lock = {0};

void k_idle() {
  while (1)
    ;
}

extern void switch_and_run_stack(uintptr_t stack);

void enqueue_thread(thread_t *thread) {
  LOCK(sched_lock);

  if (thread->enqueued) {
    UNLOCK(sched_lock);
    return;
  }

  vec_push(&threads, thread);
  vec_push(&thread->mother_proc->threads, thread);
  thread->mother_proc->thread_count++;
  thread->enqueued = 1;
  thread_count++;

  UNLOCK(sched_lock);
}

thread_t *create_thread_template(char *name, uintptr_t addr, size_t time_slice,
                                 uint64_t code_segment, uint64_t data_segment,
                                 proc_t *mother_proc, int auto_enqueue) {
  thread_t *new_thread = kmalloc(sizeof(thread_t));

  LOCK(sched_lock);

  *new_thread = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = name,
      .run_once = 0,
      .mother_proc = mother_proc,
      .enqueued = 0,
      .registers =
          (registers_t){
              .cs = code_segment,
              .ss = data_segment,
              .rip = (uint64_t)addr,
              .rflags = 0x202,
              .rsp = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET,
          },
      .time_slice = (time_slice == 0) ? 5000 : time_slice,
  };

  UNLOCK(sched_lock);

  if (auto_enqueue)
    enqueue_thread(new_thread);

  return new_thread;
}

thread_t *create_kernel_thread(char *name, uintptr_t addr, size_t time_slice,
                               proc_t *mother_proc) {
  return create_thread_template(name, addr, time_slice, GDT_SEG_KCODE,
                                GDT_SEG_KDATA, mother_proc, 1);
}

void enqueue_proc(proc_t *proc) {
  LOCK(sched_lock);

  if (proc->enqueued) {
    UNLOCK(sched_lock);
    return;
  }

  vec_push(&processes, proc);
  proc->enqueued = 1;
  proc_count++;

  UNLOCK(sched_lock);
}

proc_t *create_proc(char *name) {
  proc_t *new_proc = kmalloc(sizeof(proc_t));

  LOCK(sched_lock);

  *new_proc = (proc_t){
      .name = name,
      .thread_count = 0,
      .pid = current_pid++,
      .pagemap = (uint64_t)get_kernel_pagemap(),
      .enqueued = 0,
  };

  new_proc->threads.data = kmalloc(sizeof(thread_t *));

  UNLOCK(sched_lock);

  enqueue_proc(new_proc);

  return new_proc;
}

size_t get_next_thread(size_t offset) {
  while (1) {
    if (offset < thread_count - 1)
      offset++;
    else
      offset = 0;

    thread_t *current_thread = LOCKED_READ(threads.data[offset]);

    if (LOCK_ACQUIRE(current_thread->lock)) {
      return offset;
    }
  }
}

void schedule(uint64_t rsp) {
  lapic_timer_stop();

  cpu_locals_t *locals = get_locals();

  thread_t *current_thread = threads.data[locals->last_run_thread_index];

  size_t new_index = get_next_thread(locals->last_run_thread_index);

  if (new_index == locals->last_run_thread_index) {
    lapic_eoi();
    lapic_timer_oneshot(SCHEDULE_REG, current_thread->time_slice);
    asm volatile("sti");
    while (1)
      asm volatile("hlt");
  }

  if (current_thread->run_once)
    current_thread->registers = *((registers_t *)rsp);
  else
    current_thread->run_once = 1;

  UNLOCK(current_thread->lock);

  locals->last_run_thread_index = new_index;
  current_thread = threads.data[locals->last_run_thread_index];

  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  current_thread->mother_proc->pagemap = cr3;

  asm volatile("mov %0, %%cr3" : : "r"(current_thread->mother_proc->pagemap));

  lapic_eoi();
  lapic_timer_oneshot(SCHEDULE_REG, current_thread->time_slice);

  switch_and_run_stack((uintptr_t)&current_thread->registers);
}

void await() {
  MAKE_LOCK(await_lock);

  while (!LOCKED_READ(sched_started))
    ;

  lapic_timer_oneshot(SCHEDULE_REG, 5000);

  asm volatile("sti");

  UNLOCK(await_lock);

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
    for (volatile size_t i = 0; i < 5000000; i++)
      asm volatile("nop");
  }
}

void scheduler_init(struct stivale2_struct_tag_smp *smp_info, uintptr_t addr) {
  processes.data = kcalloc(sizeof(proc_t *));
  threads.data = kcalloc(sizeof(thread_t *));

  create_proc("k_proc");

  create_kernel_thread("k_init", (uint64_t)addr, 5000, processes.data[0]);
  create_kernel_thread("k_test", (uint64_t)test, 5000, processes.data[0]);
  create_kernel_thread("k_idle", (uint64_t)k_idle, 5000, processes.data[0]);

  for (size_t i = 0; i < 3; i++)
    vec_push(&processes.data[0]->threads, threads.data[i]);

  printf("Proc: %s: %lu:\r\n%p\r\n%p\r\n%p\r\n", processes.data[0]->name,
         processes.data[0]->thread_count, processes.data[0]->threads.data[0],
         processes.data[0]->threads.data[1],
         processes.data[0]->threads.data[2]);

  sched_started = 1;

  await();
}
