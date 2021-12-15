#ifndef __MBR_H__
#define __MBR_H__

#include <drivers/ahci.h>
#include <stdint.h>
#include <vec.h>

typedef struct mbr_partition_table {
  uint8_t flags;
  uint32_t chs_start : 24;
  uint8_t system_id;
  uint32_t chs_end : 24;
  uint32_t lba_partition_start;
  uint32_t lba_sectors_count;
} __attribute__((packed)) mbr_partition_table_t;

typedef struct valid_partiton_and_drive {
  size_t portno;
  uint8_t partition_number;
  uint32_t sector_start;
  uint32_t length_in_sectors;
} valid_partiton_and_drive_t;

typedef vec_t(valid_partiton_and_drive_t) vec_valid_partiton_and_drive_t;

extern vec_valid_partiton_and_drive_t valid_partitons;

int parse_mbr();

#endif
