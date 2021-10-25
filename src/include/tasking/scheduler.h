#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stddef.h>
#include <stdint.h>

#include <boot/stivale2.h>
#include <lock.h>
#include <mm/vmm.h>
#include <registers.h>
#include <vec.h>

struct thread;

typedef struct proc {
  char *name;
  int status;
  int enqueued;
  pagemap_t *pagemap;
  size_t pid;
  size_t thread_count;
  vec_t(struct thread *) threads;
} proc_t;

typedef struct thread {
  char *name;
  int state;
  int enqueued;
  int exit_state;
  lock_t lock;
  size_t tid;
  size_t time_slice;
  uint64_t kernel_stack;
  proc_t *mother_proc;
  registers_t registers;
} thread_t;

typedef vec_t(thread_t *) vec_thread_t;
typedef vec_t(proc_t *) vec_proc_t;

extern vec_thread_t threads;
extern vec_proc_t processes;

void scheduler_init(uintptr_t addr, struct stivale2_struct_tag_smp *smp_info);
thread_t *create_kernel_thread(char *name, uintptr_t addr, size_t time_slice,
                               proc_t *mother_proc);
void await_sched_start();
void await();

#endif
