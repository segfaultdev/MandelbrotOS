#include <drivers/ahci.h>
#include <drivers/mbr.h>
#include <klog.h>
#include <mm/kheap.h>

vec_valid_partiton_and_drive_t valid_partitons;

int parse_mbr() {
  valid_partitons.data = kmalloc(sizeof(valid_partiton_and_drive_t));

  for (size_t i = 0; i < (size_t)sata_ports.length; i++) {
    uint8_t boot_sec[512];

    sata_read(i, 0, 1, boot_sec);

    if (boot_sec[0] == 0xEB && boot_sec[2] == 0x90)
      klog(3, "Found master boot record on drive %lu\r\n", i);
    else
      continue;

    mbr_partition_table_t *partitions =
        (void *)&boot_sec[0x1be]; // Offset of the first partition

    for (uint8_t j = 0; j < 4; j++) {
      if (partitions[j].system_id) {
        klog(3, "Valid partition %u on drive %zu\r\n", j, i);

        valid_partiton_and_drive_t part = (valid_partiton_and_drive_t){
            .sector_start = partitions[j].lba_partition_start,
            .length_in_sectors = partitions[j].lba_sectors_count,
            .portno = i,
            .partition_number = j,
        };

        vec_push(&valid_partitons, part);
      }
    }
  }

  if (!valid_partitons.length)
    return 1;

  return 0;
}
