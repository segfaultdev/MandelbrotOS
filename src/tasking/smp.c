#include <boot/stivale2.h>
#include <cpu_locals.h>
#include <drivers/apic.h>
#include <drivers/pit.h>
#include <klog.h>
#include <lock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <tasking/scheduler.h>
#include <tasking/smp.h>

size_t inited_cpus = 0;

void core_init(struct stivale2_smp_info *smp_info) {
  smp_info =
      (struct stivale2_smp_info *)((uintptr_t)smp_info + PHYS_MEM_OFFSET);

  load_gdt();
  load_idt();
  vmm_switch_map_to_kern();

  cpu_locals_t *local = kcalloc(sizeof(cpu_locals_t));
  local->cpu_number = smp_info->extra_argument;
  local->last_run_thread_index = 0;
  local->lapic_id = smp_info->lapic_id;
  set_locals(local);

  load_tss((uintptr_t)&local->tss);

  local->tss.rsp0 = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET;
  local->tss.ist1 = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET;

  lapic_timer_get_freq();
  init_lapic();

  klog(3, "Brought up cpu #%lu\r\n", smp_info->extra_argument);
  LOCKED_INC(inited_cpus);

  await();
}

int init_smp(struct stivale2_struct_tag_smp *smp_info) {
  uint8_t bsp_lapic_id = smp_info->bsp_lapic_id;

  for (size_t i = 0; i < smp_info->cpu_count; i++) {
    if (smp_info->smp_info[i].lapic_id == bsp_lapic_id) {
      cpu_locals_t *local = kcalloc(sizeof(cpu_locals_t));
      local->cpu_number = i;
      local->last_run_thread_index = 0;
      local->lapic_id = bsp_lapic_id;
      set_locals(local);

      load_tss((uintptr_t)&local->tss);

      local->tss.rsp0 = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET;
      local->tss.ist1 = (uint64_t)pcalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET;

      lapic_timer_get_freq();
      init_lapic();

      klog(3, "Brought up cpu #%lu\r\n", i);
      LOCKED_INC(inited_cpus);

      continue;
    }

    smp_info->smp_info[i].extra_argument = i;
    smp_info->smp_info[i].target_stack =
        (uint64_t)pmalloc(1) + PAGE_SIZE + PHYS_MEM_OFFSET;
    smp_info->smp_info[i].goto_address = (uint64_t)core_init;
  }

  while (LOCKED_READ(inited_cpus) != smp_info->cpu_count)
    ;

  return 0;
}
