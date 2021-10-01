#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stddef.h>
#include <stdint.h>

#include <boot/stivale2.h>
#include <lock.h>
#include <registers.h>
#include <vec.h>

struct thread;

typedef struct proc {
  char *name;
  int status;
  int enqueued;
  uint64_t pagemap;
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
  proc_t *mother_proc;
  registers_t registers;
} thread_t;

void scheduler_init(uintptr_t addr, struct stivale2_struct_tag_smp *smp_info);
void await_sched_start();
void await();

#endif
