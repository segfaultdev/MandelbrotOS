#include <boot/stivale2.h>
#include <cpu_locals.h>
#include <drivers/apic.h>
#include <drivers/pcspkr.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <fs/vfs.h>
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
#include <sys/syscall.h>
#include <tasking/scheduler.h>
#include <vec.h>

#define ALIVE 0
#define DEAD 1

#define STACK_SIZE 0x20000
#define INITIAL_HEAP_SIZE 0x4000

size_t cpu_count;

static volatile int sched_started = 0;

size_t current_tid = 0;
size_t current_pid = 0;

vec_thread_t threads = {};
vec_proc_t processes = {};

static volatile lock_t sched_lock = {0};

extern void switch_and_run_stack(uintptr_t stack);

void sched_enqueue_thread(thread_t *thread) {
  LOCK(sched_lock);

  if (thread->enqueued) {
    UNLOCK(sched_lock);
    return;
  }

  vec_push(&threads, thread);
  vec_push(&thread->mother_proc->threads, thread);
  thread->enqueued = 1;

  for (size_t i = 0; i < cpu_count; i++)
    if (LOCKED_READ(cpu_locals[i].is_idle))
      lapic_send_ipi(cpu_locals[i].lapic_id, SCHEDULE_REG);

  UNLOCK(sched_lock);
}

thread_t *sched_create_thread(char *name, uintptr_t addr, size_t time_slice,
                              int user, int auto_enqueue, proc_t *mother_proc,
                              uint64_t arg1, uint64_t arg2, uint64_t arg3) {
  thread_t *new_thread = kmalloc(sizeof(thread_t));

  uint64_t stack = (uint64_t)pcalloc(STACK_SIZE / PAGE_SIZE);

  *new_thread = (thread_t){
      .tid = current_tid++,
      .exit_state = -1,
      .state = ALIVE,
      .name = name,
      .mother_proc = mother_proc,
      .enqueued = 0,
      .registers =
          (registers_t){
              .cs = (user) ? GDT_SEG_UCODE : GDT_SEG_KCODE,
              .ss = (user) ? GDT_SEG_UDATA : GDT_SEG_KDATA,
              .rip = (uint64_t)addr,
              .rflags = 0x202,
              .rdi = arg1,
              .rsi = arg2,
              .rdx = arg3,
          },
      .time_slice = (time_slice == 0) ? STANDARD_TIME_SLICE : time_slice,

      .fpu_saved_before = 0,
  };

  if (user) {
    uintptr_t stack_top = mother_proc->virtual_stack_top;
    mother_proc->virtual_stack_top -= STACK_SIZE;

    for (size_t i = 0; i < STACK_SIZE; i += PAGE_SIZE)
      vmm_map_page(mother_proc->pagemap, stack + i, stack_top + i, 0b111);

    new_thread->registers.rsp = stack_top + STACK_SIZE;

    new_thread->kernel_stack = (uint64_t)pcalloc(STACK_SIZE / PAGE_SIZE) +
                               PHYS_MEM_OFFSET + STACK_SIZE;

    mmap_range_t *mmap_range = kmalloc(sizeof(mmap_range_t));
    *mmap_range = (mmap_range_t){
        .file = NULL,
        .flags = MAP_FIXED | MAP_ANON,
        .length = STACK_SIZE,
        .offset = 0,
        .prot = PROT_READ | PROT_WRITE | PROT_EXEC,
        .phys_addr = stack,
        .virt_addr = stack_top,
    };

    vec_push(&mother_proc->pagemap->ranges, mmap_range);
  } else
    new_thread->registers.rsp = stack + PHYS_MEM_OFFSET + STACK_SIZE;

  vec_push(&mother_proc->threads, new_thread);

  if (auto_enqueue)
    sched_enqueue_thread(new_thread);

  return new_thread;
}

void sched_enqueue_proc(proc_t *proc) {
  LOCK(sched_lock);

  if (proc->enqueued) {
    UNLOCK(sched_lock);
    return;
  }

  vec_push(&processes, proc);
  proc->enqueued = 1;

  UNLOCK(sched_lock);
}

proc_t *sched_create_proc(char *name, int user) {
  proc_t *new_proc = kmalloc(sizeof(proc_t));

  *new_proc = (proc_t){
      .name = name,
      .pid = current_pid++,
      .ppid = 0,
      .pagemap = (user) ? vmm_create_new_pagemap() : &kernel_pagemap,
      .virtual_stack_top = 0x70000000000,
      .enqueued = 0,
      .last_fd = 0,
      .user = user,
      .mmap_anon_last = MMAP_ANON_START_ADDR,
  };

  new_proc->threads.data = kmalloc(sizeof(thread_t *));
  new_proc->fds.data = kmalloc(sizeof(syscall_file_t *));
  new_proc->children.data = kmalloc(sizeof(proc_t *));

  sched_enqueue_proc(new_proc);

  return new_proc;
}

static size_t get_next_thread(size_t orig_i) {
  size_t index = orig_i + 1;

  while (1) {
    if (index >= (size_t)threads.length)
      index = 0;

    thread_t *thread = LOCKED_READ(threads.data[index]);

    if (thread &&
        (LOCK_ACQUIRE(thread->lock) || thread == get_locals()->current_thread))
      return index;

    if (index == orig_i)
      break;

    index++;
  }

  return -1;
}

void schedule(uint64_t rsp) {
  vmm_load_pagemap(&kernel_pagemap);

  lapic_timer_stop();

  cpu_locals_t *locals = get_locals();
  thread_t *current_thread = locals->current_thread;

  locals->is_idle = 0;

  if (!LOCK_ACQUIRE(sched_lock)) {
    lapic_eoi();
    lapic_timer_oneshot(SCHEDULE_REG,
                        current_thread ? current_thread->time_slice : 20000);
    switch_and_run_stack((uintptr_t)rsp);
  }

  size_t new_index = get_next_thread(locals->last_run_thread_index);

  if (current_thread) {
    if (new_index == locals->last_run_thread_index) {
      UNLOCK(sched_lock);
      lapic_eoi();
      lapic_timer_oneshot(SCHEDULE_REG,
                          current_thread ? current_thread->time_slice : 20000);
      switch_and_run_stack((uintptr_t)rsp);
    }

    asm volatile("fxsave %0" : "+m"(current_thread->fpu_storage) : : "memory");
    current_thread->registers = *((registers_t *)rsp);
    UNLOCK(current_thread->lock);
  }

  if (new_index == (size_t)-1) {
    locals->current_thread = NULL;
    locals->last_run_thread_index = 0;
    locals->is_idle = 1;

    lapic_eoi();

    UNLOCK(sched_lock);

    asm volatile("hlt");
    while (1)
      ;
  }

  current_thread = LOCKED_READ(threads.data[new_index]);
  locals->current_thread = current_thread;
  locals->last_run_thread_index = new_index;

  if (current_thread->fpu_saved_before)
    asm volatile("fxrstor %0" : : "m"(current_thread->fpu_storage) : "memory");
  else
    current_thread->fpu_saved_before = 1;

  locals->tss.rsp[0] = current_thread->kernel_stack;

  lapic_eoi();
  lapic_timer_oneshot(SCHEDULE_REG, current_thread->time_slice);

  UNLOCK(sched_lock);

  vmm_load_pagemap(current_thread->mother_proc->pagemap);

  switch_and_run_stack((uintptr_t)&current_thread->registers);
}

void await() {
  asm volatile("cli");

  lapic_timer_oneshot(SCHEDULE_REG, 20000);

  asm volatile("sti\n"
               "1:\n"
               "hlt\n"
               "jmp 1b\n");
}

void sched_dequeue_thread(thread_t *thread) {
  if (!thread->enqueued)
    return;

  LOCK(sched_lock);

  while (1)
    if (LOCK_ACQUIRE(thread->lock)) {
      thread->enqueued = 0;
      vec_remove(&threads, thread);
      vec_remove(&thread->mother_proc->threads, thread);
      UNLOCK(sched_lock);
      return;
    }

  UNLOCK(sched_lock);
}

void sched_destroy_thread(thread_t *thread) {
  if (thread->tid == get_locals()->current_thread->tid)
    return;
  if (thread->enqueued)
    sched_dequeue_thread(thread);
  kfree(thread);
}

int dequeue_proc(proc_t *proc) {
  if (!proc->enqueued)
    return 0;

  LOCK(sched_lock);

  vec_remove(&processes, proc);

  UNLOCK(sched_lock);

  return 1;
}

void sched_destroy_proc(proc_t *proc) {
  for (size_t i = 0; i < (size_t)proc->threads.length; i++)
    sched_destroy_thread(proc->threads.data[i]);

  for (size_t i = 0; i < (size_t)proc->fds.length; i++) {
    syscall_file_t *file = proc->fds.data[i];

    if (file->dirty && file->is_buffered)
      vfs_write(file->file, file->buf, 0, file->file->length);

    kfree(file->buf);
    vfs_close(file->file);
  }

  if (proc->enqueued)
    dequeue_proc(proc);

  LOCK(sched_lock);

  thread_t *thread = get_locals()->current_thread;
  get_locals()->current_thread = NULL;

  thread->enqueued = 0;
  vec_remove(&threads, thread);
  vec_remove(&thread->mother_proc->threads, thread);
  kfree(thread);
  kfree(proc);

  UNLOCK(sched_lock);
  await();
}

void await_sched_start() {
  while (!LOCKED_READ(sched_started))
    ;

  await();
}

size_t sched_fork(registers_t *regs) {
  proc_t *orig_proc = get_locals()->current_thread->mother_proc;
  thread_t *orig_thread = get_locals()->current_thread;

  proc_t *new_proc = kmalloc(sizeof(proc_t));
  thread_t *new_thread = kmalloc(sizeof(thread_t));

  *new_proc = (proc_t){
      .name = strdup(orig_proc->name),
      .pid = current_pid++,
      .ppid = orig_proc->pid,
      .pagemap = vmm_fork_pagemap(orig_proc->pagemap),
      .virtual_stack_top = orig_proc->virtual_stack_top,
      .enqueued = 0,
      .last_fd = orig_proc->last_fd,
      .user = orig_proc->user,
      .mmap_anon_last = orig_proc->mmap_anon_last,
  };

  new_proc->threads.data = kmalloc(sizeof(thread_t *));
  new_proc->fds.data = kmalloc(sizeof(syscall_file_t *));
  new_proc->children.data = kmalloc(sizeof(proc_t *));

  for (size_t i = 0; i < (size_t)orig_proc->fds.length; i++) {
    syscall_file_t *sfile = kmalloc(sizeof(syscall_file_t));
    *sfile = *orig_proc->fds.data[i];
    vec_push(&new_proc->fds, sfile);
  }

  *new_thread = (thread_t){
      .registers =
          (registers_t){
              .r15 = regs->r15,
              .r14 = regs->r14,
              .r13 = regs->r13,
              .r12 = regs->r12,
              .r11 = regs->r11,
              .r10 = regs->r10,
              .r9 = regs->r9,
              .r8 = regs->r8,
              .rbp = regs->rbp,
              .rdi = regs->rdi,
              .rsi = regs->rsi,
              .rdx = regs->rdx,
              .rcx = regs->rcx,
              .rbx = regs->rbx,
              .rax = 0,
              .reserved1 = SCHEDULE_REG,
              .reserved2 = 0,
              .rip = regs->rip,
              .cs = regs->cs,
              .rflags = regs->rflags,
              .rsp = regs->rsp,
              .ss = regs->ss,
          },
      .mother_proc = new_proc,
      .time_slice = orig_thread->time_slice,
      .kernel_stack = (uintptr_t)pcalloc(STACK_SIZE / PAGE_SIZE) + STACK_SIZE +
                      PHYS_MEM_OFFSET,
      .fpu_saved_before = 0,
      .name = strdup(orig_thread->name),
      .tid = current_tid++,
      .enqueued = 0,
  };

  memcpy(new_thread->fpu_storage, orig_thread->fpu_storage, 512);
  memcpy((void *)new_thread->kernel_stack - STACK_SIZE,
         (void *)orig_thread->kernel_stack - STACK_SIZE, STACK_SIZE);

  vec_push(&orig_proc->children, new_proc);

  sched_enqueue_thread(new_thread);
  sched_enqueue_proc(new_proc);

  return new_proc->pid;
}

void scheduler_init(uintptr_t addr, struct stivale2_struct_tag_smp *smp_info) {
  processes.data = kcalloc(sizeof(proc_t *));
  threads.data = kcalloc(sizeof(thread_t *));

  proc_t *kernel_proc = sched_create_proc("k_proc", 0);

  sched_create_thread("k_init", addr, STANDARD_TIME_SLICE, 0, 1, kernel_proc, 0,
                      0, 0);

  cpu_count = smp_info->cpu_count;

  LOCKED_WRITE(sched_started, 1);

  await();
}
