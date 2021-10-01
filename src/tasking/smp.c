#include <boot/stivale2.h>
#include <cpu_locals.h>
#include <drivers/apic.h>
#include <klog.h>
#include <lock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <printf.h>
#include <stdint.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <tasking/scheduler.h>
#include <tasking/smp.h>

cpu_locals_t *cpu_locals;
static uint32_t bsp_lapic_id;
static size_t inited_cpus = 0;

void init_cpu(struct stivale2_smp_info *smp_info) {
  /* smp_info = (void *)smp_info + PHYS_MEM_OFFSET; */
  /* smp_info += PHYS_MEM_OFFSET; */

  if (smp_info->lapic_id != bsp_lapic_id) {
    load_gdt();
    load_idt();
    /* vmm_switch_map_to_kern(); */
  }

  cpu_locals_t *locals = (cpu_locals_t *)smp_info->extra_argument;

  locals->last_run_thread_index = 0;
  locals->lapic_id = smp_info->lapic_id;
  locals->current_thread = NULL;

  set_locals(locals);

  init_lapic();
  lapic_timer_get_freq();

  LOCKED_INC(inited_cpus);

  klog(3, "Brought up CPU #%lu\r\n", locals->cpu_number);

  if (smp_info->lapic_id != bsp_lapic_id)
    await_sched_start();

  return;
}

int init_smp(struct stivale2_struct_tag_smp *smp_info) {
  bsp_lapic_id = smp_info->bsp_lapic_id;
  cpu_locals = kmalloc(sizeof(cpu_locals_t) * smp_info->cpu_count);

  for (size_t i = 0; i < smp_info->cpu_count; i++) {
    cpu_locals[i].cpu_number = i;

    LOCKED_WRITE(smp_info->smp_info[i].extra_argument,
                 (uint64_t)&cpu_locals[i]);

    if (smp_info->smp_info[i].lapic_id == bsp_lapic_id) {
      init_cpu(&smp_info->smp_info[i]);
      continue;
    }

    LOCKED_WRITE(smp_info->smp_info[i].target_stack,
                 (uint64_t)pcalloc(1) + PHYS_MEM_OFFSET + PAGE_SIZE);
    LOCKED_WRITE(smp_info->smp_info[i].goto_address, (uint64_t)init_cpu);
  }

  while (LOCKED_READ(inited_cpus) != smp_info->cpu_count)
    ;

  return 0;
}
