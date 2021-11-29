#include <acpi/acpi.h>
#include <acpi/tables.h>
#include <klog.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vec.h>

rsdp_t *rsdp;
rsdt_t *rsdt;
xsdt_t *xsdt;
mcfg_t *mcfg;
madt_t *madt;

vec_madt_lapic madt_lapics;
vec_madt_ioapic madt_ioapics;
vec_madt_nmi madt_nmis;
vec_madt_iso madt_isos;

// TODO: ACPI 2.0+ support

bool do_acpi_checksum(sdt_t *th) {
  uint8_t sum = 0;

  for (uint32_t i = 0; i < th->length; i++)
    sum += ((char *)th)[i];

  return sum == 0;
}

void *get_table(char *signature, int index) {
  size_t entries;

  if (rsdp->revision <= 2)
    entries = (rsdt->h.length - sizeof(rsdt->h)) / 4;
  else
    entries = (xsdt->h.length - sizeof(xsdt->h)) / 8;

  int i = 0;
  sdt_t *h;

  for (size_t t = 0; t < entries; t++) {
    if (rsdp->revision <= 2)
      h = (sdt_t *)(uint64_t)(rsdt->sptr[t] + PHYS_MEM_OFFSET);
    else
      h = (sdt_t *)(uint64_t)(xsdt->sptr[t] + PHYS_MEM_OFFSET);

    if (!strncmp(signature, h->signature, 4)) {
      if (do_acpi_checksum(h) && i == index)
        return (void *)h;

      i++;
    }
  }

  return NULL;
}

void gather_madt() {
  madt = get_table("APIC", 0);

  if (!madt) {
    klog(1, "Failed to get MADT. Halting!");
    while (1)
      ;
  }

  madt_ioapics.data = kmalloc(sizeof(madt_ioapic_t *));
  madt_lapics.data = kmalloc(sizeof(madt_lapic_t *));
  madt_nmis.data = kmalloc(sizeof(madt_nmi_t *));
  madt_isos.data = kmalloc(sizeof(madt_iso_t *));

  for (size_t i = 0; i < (madt->header.length - (sizeof(madt_t)));) {
    madt_header_t *header = (madt_header_t *)&madt->entries[i];

    switch (header->id) {
      case 0:
        vec_push(&madt_lapics, (madt_lapic_t *)header);
        break;

      case 1:
        vec_push(&madt_ioapics, (madt_ioapic_t *)header);
        break;
      case 2:
        vec_push(&madt_nmis, (madt_nmi_t *)header);
        break;
      case 3:
        vec_push(&madt_isos, (madt_iso_t *)header);
        break;
    }

    i += header->length;
  }
}

int init_acpi(struct stivale2_struct_tag_rsdp *rsdp_info) {
  rsdp = (rsdp_t *)rsdp_info->rsdp;

  if (!rsdp->rsdt_address) {
    klog(1, "Failed to get MADT. Halting!");
    while (1)
      ;
  }

  rsdt = (rsdt_t *)(rsdp->rsdt_address + PHYS_MEM_OFFSET);

  if (rsdp->revision >= 2)
    xsdt = (xsdt_t *)(rsdp->xsdt_address + PHYS_MEM_OFFSET);

  gather_madt();

  return 0;
}
