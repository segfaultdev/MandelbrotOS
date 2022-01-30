#include <acpi/acpi.h>
#include <mm/pmm.h>
#include <pci/pci.h>
#include <stddef.h>
#include <stdint.h>

static mcfg_entry_t *get_entry(uint16_t segment, uint8_t bus) {
  for (size_t i = 0;
       i < (mcfg->h.length - sizeof(mcfg_t)) / sizeof(mcfg_entry_t); i++)
    if (mcfg->entries[i].seg_grp == segment && mcfg->entries[i].sbus <= bus &&
        mcfg->entries[i].ebus >= bus)
      return &mcfg->entries[i];
  return NULL;
}

static uintptr_t get_device_addr(mcfg_entry_t *entry, uint8_t bus,
                                 uint8_t device, uint8_t function) {
  return (entry->base_address + PHYS_MEM_OFFSET +
          (((bus - entry->sbus) << 20) | (device << 15) | (function << 12)));
}

uint8_t pci_read_8(pci_device_t *dev, uint16_t offset) {
  return *(volatile uint8_t *)(get_device_addr(
                                   get_entry(dev->segment, dev->bus), dev->bus,
                                   dev->device, dev->function) +
                               offset);
}

uint16_t pci_read_16(pci_device_t *dev, uint16_t offset) {
  return *(volatile uint16_t *)(get_device_addr(
                                    get_entry(dev->segment, dev->bus), dev->bus,
                                    dev->device, dev->function) +
                                offset);
}

uint32_t pci_read_32(pci_device_t *dev, uint16_t offset) {
  return *(volatile uint32_t *)(get_device_addr(
                                    get_entry(dev->segment, dev->bus), dev->bus,
                                    dev->device, dev->function) +
                                offset);
}

void pci_write_8(pci_device_t *dev, uint16_t offset, uint8_t data) {
  *(volatile uint8_t *)(get_device_addr(get_entry(dev->segment, dev->bus),
                                        dev->bus, dev->device, dev->function) +
                        offset) = data;
}

void pci_write_16(pci_device_t *dev, uint16_t offset, uint16_t data) {
  *(volatile uint16_t *)(get_device_addr(get_entry(dev->segment, dev->bus),
                                         dev->bus, dev->device, dev->function) +
                         offset) = data;
}

void pci_write_32(pci_device_t *dev, uint16_t offset, uint32_t data) {
  *(volatile uint32_t *)(get_device_addr(get_entry(dev->segment, dev->bus),
                                         dev->bus, dev->device, dev->function) +
                         offset) = data;
}

pci_bar_t pci_get_bar(pci_device_t *dev, uint8_t barno) {
  pci_bar_t bar;

  uint32_t bar_val = pci_read_32(dev, 0x10 + (barno * 4));
  bar.type = bar_val & 1;

  if (bar.type)
    bar.base = bar_val & 0xfffffffc;
  else {
    uint8_t bitness = (bar_val >> 1) & 3;
    switch (bitness) {
      case 1: // 16 bits
        bar.base = bar_val & 0xfff0;
        break;
      case 0: // 32 bits
        bar.base = bar_val & 0xfffffff0;
        break;
      case 2: // 64 bits
        bar.base = ((uint64_t)pci_read_32(dev, ((0x10 + (bar_val * 4)) + 4)))
                       << 32 |
                   (bar_val & 0xfffffff0);
        break;
    }
  }

  pci_write_32(dev, 0x10 + (barno * 4), 0xffffffff);

  bar.size = pci_read_32(dev, 0x10 + (barno * 4));
  bar.size &= (bar.type) ? 0xfffffffc : 0xfffffff0;
  bar.size = ~bar.size + 1;

  pci_write_32(dev, 0x10 + (barno * 4), bar_val);

  return bar;
}
