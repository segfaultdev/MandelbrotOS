#include <drivers/mbr.h>
#include <fb/fb.h>
#include <fs/fat32.h>
#include <klog.h>
#include <mm/pmm.h>
#include <printf.h>

vec_t(fat_partition_t) fat_parts;

uint64_t fat_cluster_to_sector(size_t part, uint32_t cluster) {
  return ((cluster - 2) * fat_parts.data[part].boot.cluster_size) +
         (fat_parts.data[part].boot.reserved_sectors +
          (fat_parts.data[part].boot.table_count *
           fat_parts.data[part].boot.sectors_per_fat) +
          (((fat_parts.data[part].boot.root_entries * 32) +
            (fat_parts.data[part].boot.sector_size - 1)) /
           fat_parts.data[part].boot.sector_size));
}

uint32_t fat_get_next_cluster(size_t part, uint32_t cluster) {
  uint8_t buf[512];
  uint64_t fat_sector = fat_parts.data[part].sector_start +
                        fat_parts.data[part].boot.reserved_sectors +
                        ((cluster * 4) / 512);
  uint32_t fat_offset = (cluster * 4) % 512;

  sata_read(fat_parts.data[part].drive, fat_sector, 1, buf);
  uint8_t *cluster_chain = (uint8_t *)buf;

  return *((uint32_t *)&cluster_chain[fat_offset]) & 0x0FFFFFFF;
}

uint32_t fat_load_cluster(size_t part, uint8_t *buffer, uint32_t cluster) {
  sata_read(fat_parts.data[part].drive,
            fat_cluster_to_sector(part, cluster) +
                fat_parts.data[part].sector_start,
            fat_parts.data[part].boot.cluster_size, buffer);

  return fat_get_next_cluster(part, cluster);
}

void fat_list_dir(size_t part, uint32_t directory) {
  fat_partition_t partition = fat_parts.data[part];

  if (!directory)
    directory = partition.boot.root_cluster;

  uint8_t *buffer = kmalloc(512 * partition.boot.cluster_size);
  fat_load_cluster(part, buffer, directory);

  fat_cluster_t *cluster;

  while (1) {
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

uint32_t fat_find(size_t part, uint32_t directory,
                  fat_cluster_t *parent_cluster, char *path) {
  fat_partition_t partition = fat_parts.data[part];

  if (directory >= 0x0FFFFFF7)
    return 0x0FFFFFFF;
  if (!directory)
    directory = partition.boot.root_cluster;
  if (!path)
    return directory;
  if (!strlen(path))
    return directory;

  if (path[0] == ' ' || path[0] == '/')
    return fat_find(part, directory, parent_cluster, ++path);

  char short_name[12] = "            ";
  uint8_t pos = 0;

  while (*path) {
    if (pos == 11)
      break;
    if (*path == '/')
      break;

    if (*path == '.') {
      if (pos < 8)
        pos = 8;
    } else if (*path >= 'a' && *path <= 'z')
      short_name[pos++] = (*path - 'a') + 'A';
    else
      short_name[pos++] = *path;

    path++;
  }

  size_t size = partition.boot.cluster_size * 512;
  uint8_t *buffer = kmalloc(size);
  fat_load_cluster(part, buffer, directory);

  while (directory >= 2 && directory < 0x0FFFFFF7) {
    for (size_t i = 0; i < size / 32; i++, buffer += 32) {
      fat_cluster_t *cluster = (void *)buffer;

      if (!cluster->filename[0] || cluster->filename[0] == 0xE5)
        continue;

      if (cluster->flags == 0xf) {
        long_filename_t *lfn = (void *)buffer;
        size_t count = 0;

        while (lfn->flags != 0x41) {
          count++;
          buffer += 32;
          lfn = (void *)buffer;
        }

        buffer -= 32;
        lfn = (void *)buffer;

        uint8_t *long_filename = kmalloc(
            count *
            13); // Each long file name can hold 13 characters (If you convert
                 // it from normal UTF16 to ASCII because nobody likes UTF16)

        for (size_t j = 0; j < count; j++) {
          for (size_t i = 0; i < 5; i++)
            long_filename[j * 13 + i] = lfn->filename_lo[i];
          for (size_t i = 5; i < 11; i++)
            long_filename[j * 13 + i] = lfn->filename_mid[i - 5];
          for (size_t i = 11; i < 13; i++)
            long_filename[j * 13 + i] = lfn->filename_hi[i - 11];

          buffer -= 32;
          lfn = (void *)buffer;
        }

        i += count + 2;
        buffer += (count + 1) * 32;
        fat_cluster_t *cluster = (void *)buffer;

        uint8_t *path_name = kmalloc(count * 13);

        for (size_t i = 0; i < count * 13; i++)
          path_name[i] = 255;

        path -= pos;

        pos = 0;
        while (*path) {
          if (*path == '/')
            break;

          path_name[pos++] = *path;

          path++;
        }

        path_name[pos] = 0;

        buffer -= 32;

        if (!strncmp((char *)path_name, (char *)long_filename, count * 13)) {
          uint32_t cluster_no = ((uint32_t)(cluster->cluster_hi) << 16) |
                                (uint32_t)(cluster->cluster_lo);

          if (*path == '/' && cluster->directory) {
            kfree(path_name);
            kfree(long_filename);
            kfree(buffer);

            cluster_no = fat_find(part, cluster_no, parent_cluster, path + 1);
          } else {
            kfree(path_name);
            kfree(long_filename);
            kfree(buffer);

            if (parent_cluster)
              *parent_cluster = *cluster;
          }

          return cluster_no;
        }

        kfree(path_name);
        kfree(long_filename);

      } else {
        if (!strncmp(short_name, (char *)cluster->filename, 11)) {
          uint32_t cluster_no = ((uint32_t)(cluster->cluster_hi) << 16) |
                                (uint32_t)(cluster->cluster_lo);

          if (*path == '/' && cluster->directory) {
            kfree(buffer);
            cluster_no = fat_find(part, cluster_no, parent_cluster, path + 1);
          } else {
            kfree(buffer);

            if (parent_cluster)
              *parent_cluster = *cluster;
          }

          return cluster_no;
        }
      }
    }

    buffer -= size;
    directory = fat_load_cluster(part, buffer, directory);
  }

  kfree(buffer);
  return 0x0FFFFFFF;
}

int init_fat() {
  fat_parts.data = kmalloc(sizeof(fat_partition_t));

  for (size_t i = 0; i < (size_t)valid_partitons.length; i++) {
    bpb_t bios_block;
    sata_read(valid_partitons.data[i].portno,
              valid_partitons.data[i].sector_start, 1, (uint8_t *)&bios_block);

    if (bios_block.signature == 0x28 || bios_block.signature == 0x29) {
      klog(3, "Found valid FAT32 signature on drive %zu partition %u\r\n",
           valid_partitons.data[i].portno,
           valid_partitons.data[i].partition_number);

      fat_partition_t partition = (fat_partition_t){
          .boot = bios_block,
          .drive = valid_partitons.data[i].portno,
          .partiton = valid_partitons.data[i].partition_number,
          .sector_start = valid_partitons.data[i].sector_start,
          .sector_length = valid_partitons.data[i].length_in_sectors,
      };

      vec_push(&fat_parts, partition);
    }
  }

  if (!fat_parts.length)
    return 1;

  fat_list_dir(0, fat_find(0, 0, NULL, "/boot"));
  puts("\r\nContents of limine.cfg:\r\n");

  fat_cluster_t cluster;
  uint32_t cluster_no = fat_find(0, 0, &cluster, "/boot/limine.cfg");
  uint8_t *buffer =
      kmalloc((cluster.size % 512 == 0) ? cluster.size
                                        : (cluster.size / 512) * 512 + 512);

  fat_load_cluster(0, buffer, cluster_no);

  for (size_t i = 0; i < cluster.size; i++) {
    if (buffer[i] == '\n')
      puts("\r\n");
    else
      putchar(buffer[i]);
  }

  return 0;
}
