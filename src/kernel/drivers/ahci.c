#include <acpi/acpi.h>
#include <device.h>
#include <drivers/ahci.h>
#include <drivers/mbr.h>
#include <fs/vfs.h>
#include <klog.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <pci/pci.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vec.h>

#define AHCI_BAR5_OFFSET 0x24

#define SATA_SIG_ATA 0x00000101
#define SATA_SIG_ATAPI 0xEB140101
#define SATA_SIG_SEMB 0xC33C0101
#define SATA_SIG_PM 0x96690101

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define HBA_PXCMD_ST 0x0001
#define HBA_PXCMD_FRE 0x0010
#define HBA_PXCMD_FR 0x4000
#define HBA_PXCMD_CR 0x8000

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35

#define HBA_PXIS_TFES (1 << 30)
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

vec_t(uint64_t) abars = {};

void ahci_find_abar(uint64_t base_address, uint64_t bus) {
  uint64_t bus_address = base_address + (bus << 20);
  pci_device_t *pci_bus = (pci_device_t *)bus_address;

  if (!pci_bus->device_id || pci_bus->device_id == 0xFFFF)
    return;

  for (uint8_t device = 0; device < 32; device++) {
    uint64_t device_address = bus_address + (device << 15);
    pci_device_t *pci_device = (pci_device_t *)device_address;

    if (!pci_device->device_id || pci_device->device_id == 0xFFFF ||
        pci_device->header_type & 0b1111111)
      continue;

    uint8_t functions = pci_device->header_type & (1 << 7) ? 8 : 1;

    for (uint8_t function = 0; function < functions; function++) {
      uint64_t function_address = device_address + (function << 12);
      pci_device_t *pci_fn = (pci_device_t *)function_address;

      if (!pci_fn->device_id || pci_fn->device_id == 0xFFFF)
        continue;
      if (pci_fn->class == 1 && pci_fn->subclass == 6)
        vec_push(&abars, pci_cfg_read_dword(0, bus, device, function,
                                            AHCI_BAR5_OFFSET));
    }
  }
}

// Thanks to OSDEV.org for these 5 snippets
static inline uint8_t ahci_check_type(hba_port_t *port) {
  uint32_t ssts = port->ssts;
  uint8_t ipm = (ssts >> 8) & 0x0F;
  uint8_t det = ssts & 0x0F;

  if (det != HBA_PORT_DET_PRESENT)
    return AHCI_DEV_NULL;
  if (ipm != HBA_PORT_IPM_ACTIVE)
    return AHCI_DEV_NULL;

  switch (port->sig) {
    case SATA_SIG_ATAPI:
      return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB:
      return AHCI_DEV_SEMB;
    case SATA_SIG_PM:
      return AHCI_DEV_PM;
    default:
      return AHCI_DEV_SATA;
  }
}

void ahci_start_cmd_engine(hba_port_t *port) {
  while (port->cmd & HBA_PXCMD_CR)
    ;

  port->cmd |= HBA_PXCMD_FRE;
  port->cmd |= HBA_PXCMD_ST;
}

void ahci_stop_cmd_engine(hba_port_t *port) {
  port->cmd &= ~HBA_PXCMD_ST;
  port->cmd &= ~HBA_PXCMD_FRE;

  while (1) {
    if (port->cmd & HBA_PXCMD_FR)
      continue;
    if (port->cmd & HBA_PXCMD_CR)
      continue;
    break;
  }
}

void ahci_port_init(hba_port_t *port) {
  ahci_stop_cmd_engine(port);

  port->clb = (uint32_t)(uint64_t)pcalloc(1);
  port->clbu = 0;

  port->fb = (uint32_t)(uint64_t)pcalloc(1);
  port->fbu = 0;

  hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)(uint64_t)(port->clb);

  for (int i = 0; i < 32; i++) {
    cmd_header[i].prdtl = 8;
    cmd_header[i].ctba = (uint32_t)(uint64_t)pcalloc(1);
    cmd_header[i].ctbau = 0;
  }

  ahci_start_cmd_engine(port);
}

int8_t ahci_find_cmdslot(hba_port_t *port) {
  uint32_t slots = (port->sact | port->ci);

  for (int i = 0; i < 32; i++) {
    if (!(slots & 1))
      return i;
    slots >>= 1;
  }

  return -1;
}
// End snippets

uint8_t sata_read(device_t *dev, uint64_t start, uint32_t count, uint8_t *buf) {
  hba_port_t *port = dev->private_data;

  uint32_t startl = (uint32_t)start;
  uint32_t starth = (uint32_t)(start >> 32);

  port->is = (uint32_t)-1;

  int8_t slot = ahci_find_cmdslot(port);
  if (slot == -1)
    return 1;

  hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)(uintptr_t)(port->clb);
  cmd_header += slot;
  cmd_header->cfl = sizeof(fis_reg_host_to_device_t) / sizeof(uint32_t);
  cmd_header->w = 0;
  cmd_header->prdtl = (uint16_t)((count - 1) >> 4) + 1;

  hba_cmd_tbl_t *cmd_table = (hba_cmd_tbl_t *)(uintptr_t)cmd_header->ctba;
  memset(cmd_table, 0,
         sizeof(hba_cmd_tbl_t) +
             (cmd_header->prdtl - 1) * sizeof(hba_prdt_entry_t));

  size_t i;
  for (i = 0; i < (size_t)cmd_header->prdtl - 1; i++) {
    cmd_table->prdt_entry[i].dba = (uint32_t)(uintptr_t)(buf - PHYS_MEM_OFFSET);
    cmd_table->prdt_entry[i].dbau = 0;
    cmd_table->prdt_entry[i].dbc = 8 * 1024 - 1; // 8KiB - 1
    cmd_table->prdt_entry[i].i = 1;

    buf += 8 * 1024;
    count -= 16;
  }

  cmd_table->prdt_entry[i].dba =
      (uint32_t)(uintptr_t)((uint64_t)buf - PHYS_MEM_OFFSET);
  cmd_table->prdt_entry[i].dbc = (count << 9) - 1;
  cmd_table->prdt_entry[i].i = 1;

  fis_reg_host_to_device_t *cmd_fis =
      (fis_reg_host_to_device_t *)(&cmd_table->cfis);

  cmd_fis->fis_type = FIS_TYPE_REG_HOST_TO_DEVICE;
  cmd_fis->c = 1;
  cmd_fis->command = ATA_CMD_READ_DMA_EX;

  cmd_fis->lba0 = (uint8_t)startl;
  cmd_fis->lba1 = (uint8_t)(startl >> 8);
  cmd_fis->lba2 = (uint8_t)(startl >> 16);
  cmd_fis->device = 1 << 6;

  cmd_fis->lba3 = (uint8_t)(startl >> 24);
  cmd_fis->lba4 = (uint8_t)(starth);
  cmd_fis->lba5 = (uint8_t)(starth >> 8);

  cmd_fis->countl = (count & 0xFF);
  cmd_fis->counth = (count >> 8);

  for (uint32_t spin = 0; spin < 1000000; spin++) {
    if (!(port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)))
      break;
  }
  if ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)))
    return 1;

  port->ci = (1 << slot);

  while (1) {
    if (!(port->ci & (1 << slot)))
      break;
    if (port->is & HBA_PXIS_TFES)
      return 1;
  }

  if (port->is & HBA_PXIS_TFES)
    return 1;

  return 0;
}

uint8_t sata_write(device_t *dev, uint64_t start, uint32_t count,
                   uint8_t *buf) {
  hba_port_t *port = dev->private_data;

  uint32_t startl = (uint32_t)start;
  uint32_t starth = (uint32_t)(start >> 32);

  port->is = (uint32_t)-1;

  int8_t slot = ahci_find_cmdslot(port);
  if (slot == -1)
    return 1;

  hba_cmd_header_t *cmd_header = (hba_cmd_header_t *)(uintptr_t)(port->clb);
  cmd_header += slot;
  cmd_header->cfl = sizeof(fis_reg_host_to_device_t) / sizeof(uint32_t);
  cmd_header->w = 1;
  cmd_header->c = 1;
  cmd_header->p = 1;
  cmd_header->prdtl = (uint16_t)((count - 1) >> 4) + 1;

  hba_cmd_tbl_t *cmd_table = (hba_cmd_tbl_t *)(uintptr_t)cmd_header->ctba;
  memset(cmd_table, 0,
         sizeof(hba_cmd_tbl_t) +
             (cmd_header->prdtl - 1) * sizeof(hba_prdt_entry_t));

  size_t i;
  for (i = 0; i < (size_t)cmd_header->prdtl - 1; i++) {
    cmd_table->prdt_entry[i].dba = (uint32_t)(uintptr_t)(buf - PHYS_MEM_OFFSET);
    cmd_table->prdt_entry[i].dbau = 0;
    cmd_table->prdt_entry[i].dbc = 8 * 1024 - 1; // 8KiB - 1
    cmd_table->prdt_entry[i].i = 1;

    buf += 8 * 1024;
    count -= 16;
  }

  cmd_table->prdt_entry[i].dba =
      (uint32_t)(uintptr_t)((uint64_t)buf - PHYS_MEM_OFFSET);
  cmd_table->prdt_entry[i].dbc = (count << 9) - 1;
  cmd_table->prdt_entry[i].i = 1;

  fis_reg_host_to_device_t *cmd_fis =
      (fis_reg_host_to_device_t *)(&cmd_table->cfis);

  cmd_fis->fis_type = FIS_TYPE_REG_HOST_TO_DEVICE;
  cmd_fis->c = 1;
  cmd_fis->command = ATA_CMD_WRITE_DMA_EX;

  cmd_fis->lba0 = (uint8_t)startl;
  cmd_fis->lba1 = (uint8_t)(startl >> 8);
  cmd_fis->lba2 = (uint8_t)(startl >> 16);
  cmd_fis->device = 1 << 6;

  cmd_fis->lba3 = (uint8_t)(startl >> 24);
  cmd_fis->lba4 = (uint8_t)(starth);
  cmd_fis->lba5 = (uint8_t)(starth >> 8);

  cmd_fis->countl = (count & 0xFF);
  cmd_fis->counth = (count >> 8);

  for (uint32_t spin = 0; spin < 1000000; spin++) {
    if (!(port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)))
      break;
  }
  if ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)))
    return 1;

  port->ci = (1 << slot);

  while (1) {
    if (!(port->ci & (1 << slot)))
      break;
    if (port->is & HBA_PXIS_TFES)
      return 1;
  }

  if (port->is & HBA_PXIS_TFES)
    return 1;

  return 0;
}

void ahci_init_abars() {
  for (size_t j = 0; j < (size_t)abars.length; j++) {
    hba_mem_t *abar = (hba_mem_t *)abars.data[j];

    for (uint32_t i = 0, pi = abar->pi; i < 32; i++, pi >>= 1) {
      if (pi & 1)
        if (ahci_check_type(&abar->ports[i]) == AHCI_DEV_SATA) {
          ahci_port_init(&abar->ports[i]);

          for (size_t p = 0; p < 4; p++) {
            device_t *dev = kmalloc(sizeof(device_t));
            *dev = (device_t){
                .private_data = (void *)&abar->ports[i],
                .name = "SATA",
                .type = DEVICE_BLOCK,
                .fs_type = DEVICE_FS_DISK,
                .read = sata_read,
                .write = sata_write,
            };

            partition_layout_t *part = probe_mbr(dev, p);
            if (part) {
              dev->private_data2 = (void *)part;
              device_add(dev);
            } else {
              kfree(dev->private_data);
              kfree(dev);
            }
          }

          device_t *main_dev = kmalloc(sizeof(device_t));
          *main_dev = (device_t){
              .private_data = (void *)&abar->ports[i],
              .private_data2 = NULL,
              .name = "SATA",
              .type = DEVICE_BLOCK,
              .fs_type = DEVICE_FS_DISK,
              .read = sata_read,
              .write = sata_write,
          };

          device_add(main_dev);
        }
    }
  }
}

int init_sata() {
  abars.data = kcalloc(sizeof(uint64_t));

  if (!mcfg)
    return 1;

  for (size_t i = 0;
       i < (mcfg->h.length - sizeof(mcfg_t)) / sizeof(mcfg_entry_t); i++)
    for (size_t bus = mcfg->entries[i].sbus; bus < mcfg->entries[i].ebus; bus++)
      ahci_find_abar(mcfg->entries[i].base_address + PHYS_MEM_OFFSET, bus);

  if (!abars.length)
    return 1;

  ahci_init_abars();

  return 0;
}
