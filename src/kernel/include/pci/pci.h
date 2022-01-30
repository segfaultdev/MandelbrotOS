#ifndef __PCI_H__
#define __PCI_H__

#include <acpi/tables.h>
#include <stddef.h>
#include <stdint.h>
#include <vec.h>

typedef struct pci_header {
  uint16_t vendor_id;
  uint16_t device_id;
  uint16_t command;
  uint16_t status;
  uint8_t revision_id;
  uint8_t prog_if;
  uint8_t subclass;
  uint8_t class;
  uint8_t cache_lsize;
  uint8_t latency_timer;
  uint8_t header_type;
  uint8_t bist;
} __attribute__((packed)) pci_header_t;

typedef struct pci_device {
  pci_header_t header;
  uint16_t segment;
  uint32_t bus;
  uint8_t device;
  uint8_t function;
} pci_device_t;

typedef struct pci_bar {
  uint64_t base;
  size_t size;
  int type;
} pci_bar_t;

typedef vec_t(pci_device_t *) vec_pci_device_t;

extern char *pci_device_classes[];
extern vec_pci_device_t pci_devices;

char *pci_get_subclass_name(uint8_t class, uint8_t subclass);
char *pci_get_vendor_name(uint16_t vendor_id);
char *pci_get_device_name(uint16_t vendor_id, uint16_t device_id);

int pci_enumerate();

uint8_t pci_read_8(pci_device_t *dev, uint16_t offset);
uint16_t pci_read_16(pci_device_t *dev, uint16_t offset);
uint32_t pci_read_32(pci_device_t *dev, uint16_t offset);
void pci_write_8(pci_device_t *dev, uint16_t offset, uint8_t data);
void pci_write_16(pci_device_t *dev, uint16_t offset, uint16_t data);
void pci_write_32(pci_device_t *dev, uint16_t offset, uint32_t data);
pci_bar_t pci_get_bar(pci_device_t *dev, uint8_t barno);

#endif
