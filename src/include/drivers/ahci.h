#ifndef __AHCI_H__
#define __AHCI_H__

#include <stddef.h>
#include <stdint.h>
#include <vec.h>

enum {
  FIS_TYPE_REG_HOST_TO_DEVICE = 0x27,
  FIS_TYPE_REG_DEVICE_TO_HOST = 0x34,
  FIS_TYPE_DMA_ACT = 0x39,
  FIS_TYPE_DMA_SETUP = 0x41,
  FIS_TYPE_DATA = 0x46,
  FIS_TYPE_BIST = 0x58,
  FIS_TYPE_PIO_SETUP = 0x5F,
  FIS_TYPE_DEV_BITS = 0xA1,
};

typedef struct fis_reg_host_to_device {
  uint8_t fis_type;
  uint8_t pmport : 4;
  uint8_t rsv0 : 3;
  uint8_t c : 1;
  uint8_t command;
  uint8_t featurel;
  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t device;
  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t featureh;
  uint8_t countl;
  uint8_t counth;
  uint8_t icc;
  uint8_t control;
  uint8_t rsv1[4];
} __attribute__((packed)) fis_reg_host_to_device_t;

typedef struct fis_reg_device_to_host {
  uint8_t fis_type;
  uint8_t pmport : 4;
  uint8_t rsv0 : 2;
  uint8_t i : 1;
  uint8_t rsv1 : 1;
  uint8_t status;
  uint8_t error;
  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t device;
  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t rsv2;
  uint8_t countl;
  uint8_t counth;
  uint8_t rsv3[2];
  uint8_t rsv4[4];
} __attribute__((packed)) fis_reg_device_to_host_t;

typedef struct fis_data {
  uint8_t fis_type;
  uint8_t pmport : 4;
  uint8_t rsv0 : 4;
  uint8_t rsv1[2];
  uint32_t data[1];
} __attribute__((packed)) fis_data_t;

typedef struct fis_pio_setup {
  uint8_t fis_type;
  uint8_t pmport : 4;
  uint8_t rsv0 : 1;
  uint8_t d : 1;
  uint8_t i : 1;
  uint8_t rsv1 : 1;
  uint8_t status;
  uint8_t error;
  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t device;
  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t rsv2;
  uint8_t countl;
  uint8_t counth;
  uint8_t rsv3;
  uint8_t e_status;
  uint16_t tc;
  uint8_t rsv4[2];
} __attribute__((packed)) fis_pio_setup_t;

typedef struct fis_dma_setup {
  uint8_t fis_type;
  uint8_t pmport : 4;
  uint8_t rsv0 : 1;
  uint8_t d : 1;
  uint8_t i : 1;
  uint8_t a : 1;
  uint8_t rsved[2];
  uint64_t dma_buffer_id;
  uint32_t rsvd;
  uint32_t dma_buf_offset;
  uint32_t transfer_count;
  uint32_t resvd;
} __attribute__((packed)) fis_dma_setup_t;

typedef volatile struct hba_port {
  uint32_t clb;
  uint32_t clbu;
  uint32_t fb;
  uint32_t fbu;
  uint32_t is;
  uint32_t ie;
  uint32_t cmd;
  uint32_t rsv0;
  uint32_t tfd;
  uint32_t sig;
  uint32_t ssts;
  uint32_t sctl;
  uint32_t serr;
  uint32_t sact;
  uint32_t ci;
  uint32_t sntf;
  uint32_t fbs;
  uint32_t rsv1[11];
  uint32_t vendor[4];
} __attribute__((packed)) hba_port_t;

typedef volatile struct hba_mem {
  uint32_t cap;
  uint32_t ghc;
  uint32_t is;
  uint32_t pi;
  uint32_t vs;
  uint32_t ccc_ctl;
  uint32_t ccc_pts;
  uint32_t em_loc;
  uint32_t em_ctl;
  uint32_t cap2;
  uint32_t bohc;

  uint8_t rsv[0xA0 - 0x2C];

  uint8_t vendor[0x100 - 0xA0];

  hba_port_t ports[1];
} __attribute__((packed)) hba_mem_t;

typedef struct hba_cmd_header {
  uint8_t cfl : 5;
  uint8_t a : 1;
  uint8_t w : 1;
  uint8_t p : 1;
  uint8_t r : 1;
  uint8_t b : 1;
  uint8_t c : 1;
  uint8_t rsv0 : 1;
  uint8_t pmp : 4;
  uint16_t prdtl;

  volatile uint32_t prdbc;

  uint32_t ctba;
  uint32_t ctbau;
  uint32_t rsv1[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct hba_prdt_entry {
  uint32_t dba;
  uint32_t dbau;
  uint32_t rsv0;
  uint32_t dbc : 22;
  uint32_t rsv1 : 9;
  uint32_t i : 1;
} __attribute__((packed)) hba_prdt_entry_t;

typedef struct hba_cmd_tbl {
  uint8_t cfis[64];
  uint8_t acmd[16];
  uint8_t rsv[48];
  hba_prdt_entry_t prdt_entry[1];
} __attribute__((packed)) hba_cmd_tbl_t;

typedef vec_t(hba_port_t *) vec_hba_port_t;

extern vec_hba_port_t sata_ports;

int sata_read(size_t portno, uint64_t start, uint32_t count, uint8_t *buf);
int sata_write(size_t portno, uint64_t start, uint32_t count, uint8_t *buf);
int init_sata();

#endif
