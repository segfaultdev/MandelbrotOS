#include <acpi/acpi.h>
#include <acpi/tables.h>
#include <klog.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <pci/pci.h>
#include <stddef.h>
#include <vec.h>

vec_pci_device_t pci_devices = {0};

void pci_enumerate_fn(uint64_t device_address, uint64_t bus, uint8_t device,
                      uint8_t function) {
  uint64_t function_address = device_address + (function << 12);

  pci_header_t *pci_fn = (pci_header_t *)function_address;

  if (!pci_fn->device_id || pci_fn->device_id == 0xFFFF)
    return;

  klog(3, "%x:%x.%x -> %s, %s, %s, %s\r\n", (uint32_t)bus, device, function,
       pci_get_vendor_name(pci_fn->vendor_id),
       pci_device_classes[pci_fn->class],
       pci_get_subclass_name(pci_fn->class, pci_fn->subclass),
       pci_get_device_name(pci_fn->vendor_id, pci_fn->device_id));

  pci_device_t *dev = kmalloc(sizeof(pci_device_t));
  *dev = (pci_device_t){.segment = 0,
                        .bus = bus,
                        .device = device,
                        .function = function,
                        .header = *pci_fn};

  vec_push(&pci_devices, dev);
}

void pci_enumerate_dev(uint64_t bus_address, uint64_t bus, uint8_t device) {
  uint64_t device_address = bus_address + (device << 15);

  pci_header_t *pci_device = (pci_header_t *)device_address;

  if (!pci_device->device_id || pci_device->device_id == 0xFFFF)
    return;

  // We do not handle PCI to PCI or Cardbus bridges currently
  if (pci_device->header_type & 0b1111111)
    return;

  uint8_t functions = pci_device->header_type & (1 << 7) ? 8 : 1;

  for (uint8_t function = 0; function < functions; function++)
    pci_enumerate_fn(device_address, bus, device, function);
}

void pci_enumerate_bus(uint64_t base_address, uint64_t bus) {
  uint64_t bus_address = base_address + (bus << 20);

  pci_header_t *pci_bus = (pci_header_t *)bus_address;

  if (!pci_bus->device_id || pci_bus->device_id == 0xFFFF)
    return;

  for (uint8_t device = 0; device < 32; device++)
    pci_enumerate_dev(bus_address, bus, device);
}

int pci_enumerate() {
  mcfg = (mcfg_t *)acpi_get_table("MCFG", 0);

  pci_devices.data = kmalloc(sizeof(pci_device_t *));

  if (mcfg == NULL)
    return 1;

  for (size_t t = 0;
       t < (mcfg->h.length - sizeof(mcfg_t)) / sizeof(mcfg_entry_t); t++)
    for (size_t bus = mcfg->entries[t].sbus; bus < mcfg->entries[t].ebus; bus++)
      pci_enumerate_bus(mcfg->entries[t].base_address + PHYS_MEM_OFFSET, bus);

  return 0;
}
