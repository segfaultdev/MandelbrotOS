#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stddef.h>
#include <stdint.h>

#include <boot/stivale2.h>
#include <registers.h>

struct thread;

typedef struct proc {
  char *name;
  int status;
  size_t pid;
  size_t thread_count;
  struct thread *threads;
} proc_t;

typedef struct thread {
  char *name;
  int state;
  int exit_state;
  int run_once;
  int running;
  uint64_t *pagemap;
  size_t tid;
  size_t priority;
  proc_t *mother_proc;
  registers_t registers;
} thread_t;

void scheduler_init(struct stivale2_struct_tag_smp *smp_info, uintptr_t addr);
void await();

#endif
