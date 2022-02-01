#ifndef __MBR_H__
#define __MBR_H__

#include <device.h>
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

typedef struct partition_layout {
  uint8_t partition_number;
  uint32_t sector_start;
  uint32_t length_in_sectors;
} partition_layout_t;

partition_layout_t *probe_mbr(device_t *dev, size_t num);

#endif
