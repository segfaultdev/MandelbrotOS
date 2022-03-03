#include <dev/device.h>
#include <drivers/ahci.h>
#include <drivers/mbr.h>
#include <klog.h>
#include <mm/kheap.h>
#include <printf.h>

partition_layout_t *probe_mbr(device_t *dev, size_t num) {
  uint8_t boot_sec[512];

  dev->read(dev, 0, 1, boot_sec);

  mbr_partition_table_t *partitions =
      (void *)&boot_sec[0x1be]; // Offset of the first partition

  if (!partitions[num].system_id)
    return NULL;

  partition_layout_t *part = kmalloc(sizeof(partition_layout_t));

  *part = (partition_layout_t){
      .sector_start = partitions[num].lba_partition_start,
      .length_in_sectors = partitions[num].lba_sectors_count,
      .partition_number = num,
  };

  return part;
}
