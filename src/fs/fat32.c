#include <drivers/mbr.h>
#include <fb/fb.h>
#include <fs/fat32.h>
#include <klog.h>
#include <mm/pmm.h>
#include <printf.h>

vec_t(fat_partition_t) fat_partitions;

uint64_t fat_cluster_to_sector(size_t fat_part, uint32_t cluster) {
  return ((cluster - 2) * fat_partitions.data[fat_part].boot.cluster_size) +
         (fat_partitions.data[fat_part].boot.reserved_sectors +
          (fat_partitions.data[fat_part].boot.table_count *
           fat_partitions.data[fat_part].boot.sectors_per_fat) +
          (((fat_partitions.data[fat_part].boot.root_entries * 32) +
            (fat_partitions.data[fat_part].boot.sector_size - 1)) /
           fat_partitions.data[fat_part].boot.sector_size));
}

uint32_t fat_get_next_cluster(size_t fat_part, uint32_t cluster) {
  uint8_t buf[512];
  uint64_t fat_sector = fat_partitions.data[fat_part].sector_start +
                        fat_partitions.data[fat_part].boot.reserved_sectors +
                        ((cluster * 4) / 512);
  uint32_t fat_offset = (cluster * 4) % 512;

  sata_read(fat_partitions.data[fat_part].drive, fat_sector, 1, buf);
  uint8_t *cluster_chain = (uint8_t *)buf;

  return *((uint32_t *)&cluster_chain[fat_offset]) & 0x0FFFFFFF;
}

uint32_t fat_load_cluster(size_t fat_part, uint8_t *buffer, uint32_t cluster) {
  sata_read(fat_partitions.data[fat_part].drive,
            fat_cluster_to_sector(fat_part, cluster) +
                fat_partitions.data[fat_part].sector_start,
            fat_partitions.data[fat_part].boot.cluster_size, buffer);

  return fat_get_next_cluster(fat_part, cluster);
}

void fat_list_dir(size_t fat_part, uint32_t directory) {
  fat_partition_t fat_partition = fat_partitions.data[fat_part];

  uint8_t *buffer = kmalloc(512 * fat_partition.boot.cluster_size);
  fat_load_cluster(fat_part, buffer, directory);
  fat_cluster_t *cluster;

  while (directory >= 2 && directory < 0x0FFFFFF7) {
    cluster = (void *)buffer;

    if (!cluster->filename[0]) {
      puts("End of listing!\r\n");
      break;
    } else if (cluster->filename[0] == 0xe5) {
      puts("Empty cluster!\r\n");
      continue;
    }

    if (cluster->flags == 0xf) {
      long_filename_t *lfn = (void *)buffer;
      size_t count = 0;

      while (lfn->flags != 0x41) {
        count++;
        buffer += 32;
        lfn = (void *)buffer;
      }

      cluster = (void *)buffer;
      for (uint8_t i = 0; i < 8; i++)
        putchar(cluster->filename[i]);
      putchar('.');
      for (uint8_t i = 8; i < 11; i++)
        putchar(cluster->filename[i]);

      buffer -= 32;
      lfn = (void *)buffer;

      puts(": ");

      for (size_t i = 0; i < count; i++) {
        for (size_t i = 0; i < 5; i++)
          putchar(lfn->filename_lo[i]);
        for (size_t i = 0; i < 6; i++)
          putchar(lfn->filename_mid[i]);
        for (size_t i = 0; i < 2; i++)
          putchar(lfn->filename_hi[i]);

        buffer -= 32;
        lfn = (void *)buffer;
      }

      puts("\r\n");

      buffer += (count + 2) * 32;

      continue;
    }

    if (cluster->directory) {
      for (uint8_t i = 0; i < 11; i++)
        putchar(cluster->filename[i]);

      puts(" <DIR>");
    } else {
      for (uint8_t i = 0; i < 8; i++)
        putchar(cluster->filename[i]);
      putchar('.');
      for (uint8_t i = 8; i < 11; i++)
        putchar(cluster->filename[i]);
    }

    puts("\r\n");
    buffer += 32;
  }

  kfree(buffer);
}

int init_fat() {
  fat_partitions.data = kmalloc(sizeof(fat_partition_t));

  for (size_t i = 0; i < (size_t)valid_partitons.length; i++) {
    bpb_t bios_block;
    sata_read(valid_partitons.data[i].portno,
              valid_partitons.data[i].sector_start, 1, (uint8_t *)&bios_block);

    if (bios_block.signature == 0x28 || bios_block.signature == 0x29) {
      klog(3, "Found valid FAT32 signature on drive %zu partition %u\r\n",
           valid_partitons.data[i].portno,
           valid_partitons.data[i].partition_number);

      fat_partition_t fat_partition = (fat_partition_t){
          .boot = bios_block,
          .drive = valid_partitons.data[i].portno,
          .partiton = valid_partitons.data[i].partition_number,
          .sector_start = valid_partitons.data[i].sector_start,
          .sector_length = valid_partitons.data[i].length_in_sectors,
      };

      vec_push(&fat_partitions, fat_partition);
    }
  }

  if (!fat_partitions.length)
    return 1;

  fat_list_dir(0, fat_partitions.data[0].boot.root_cluster);

  return 0;
}
