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

  sata_read(fat_parts.data[part].drive,
            fat_parts.data[part].sector_start +
                fat_parts.data[part].boot.reserved_sectors +
                ((cluster * 4) / 512),
            1, buf);

  return *((uint32_t *)&buf[cluster * 4 % 512]) & 0xfffffff;
}

uint32_t fat_read_cluster(size_t part, uint8_t *buffer, uint32_t cluster) {
  sata_read(fat_parts.data[part].drive,
            fat_cluster_to_sector(part, cluster) +
                fat_parts.data[part].sector_start,
            fat_parts.data[part].boot.cluster_size, buffer);

  return fat_get_next_cluster(part, cluster);
}

void fat_read_cluster_chain(size_t part, uint8_t *buffer, uint32_t cluster) {
  while (cluster >= 2 && cluster < 0xffffff7) {
    cluster = fat_read_cluster(part, buffer, cluster);
    buffer += fat_parts.data[part].boot.cluster_size * 512;
  }
}

size_t fat_chain_cluster_length(size_t part, uint32_t cluster) {
  uint8_t *buffer = kmalloc(512);
  size_t count = 0;

  while (cluster >= 2 && cluster < 0xffffff7) {
    cluster = fat_read_cluster(part, buffer, cluster);
    count++;
  }

  kfree(buffer);

  return count;
}

void fat_change_fat_value(size_t part, uint32_t current_cluster,
                          uint32_t next_cluster) {
  uint8_t buffer[512];
  uint64_t sector = fat_parts.data[part].sector_start +
                    fat_parts.data[part].boot.reserved_sectors +
                    ((current_cluster * 4) / 512);

  sata_read(fat_parts.data[part].drive, sector, 1, buffer);

  buffer[current_cluster * 4 % 512] = next_cluster;

  sata_write(fat_parts.data[part].drive, sector, 1, buffer);
}

uint32_t fat_find_free_cluster(size_t part) {
  for (size_t i = 0;
       i < fat_parts.data[part].boot.sectors_per_fat; i++) {
    uint32_t buffer[128];

    sata_read(fat_parts.data[part].drive,
              i + fat_parts.data[part].sector_start +
                  fat_parts.data[part].boot.reserved_sectors,
              1, (uint8_t *)buffer);

    for (uint8_t j = 0; j < 128; j++) 
      if (!buffer[j]) {
        return ((i << 7) | j) & 0x0fffffff;
      }
  }

  return 0xfffffff;
}

uint32_t fat_find_free_cluster_chain(size_t part, size_t size) {
  size_t cluster_size = fat_parts.data[part].boot.cluster_size * 512;
  size = (size + (cluster_size - 1)) / cluster_size;

  uint32_t start = 0;
  uint32_t previous = 0;
  uint32_t current = 0;

  for (size_t i = 0; i < size; i++) {
    current = fat_find_free_cluster(part);

    if (previous)
      fat_change_fat_value(part, previous, current);

    fat_change_fat_value(part, current, 0x0ffffff8);

    if (!start)
      start = current;

    previous = current;
  }

  return start;
}

void fat_free_chain(size_t part, uint32_t cluster) {
  while (1) {
    uint32_t next = fat_get_next_cluster(part, cluster);

    if (!next || next == 0x0ffffff8)
      return;

    fat_change_fat_value(part, cluster, 0);

    cluster = next;
  }
}

uint32_t fat_get_free_dir_entry_chain(size_t part, uint32_t directory, size_t length) {
  fat_partition_t partition = fat_parts.data[part];

  if (!directory)
    directory = partition.boot.root_cluster;

  size_t size = fat_chain_cluster_length(part, directory) * partition.boot.cluster_size * 512;
  size_t start = 0;
  size_t found_length = 0;

  dir_entry_t *entries = kmalloc(size);

  fat_read_cluster_chain(part, (uint8_t *)entries, directory);
  size /= sizeof(dir_entry_t);

  for (size_t i = 0; i < size; i++) {
    if (!entries[i].filename[0] || entries[i].filename[0] == 0xe5) {
      if (++found_length == length) {
        kfree(entries);
        return (!start) ? i : start;
      }
      else if (!start)
        start = i;
    }
    else {
      found_length = 0;
      start = 0;
    }
  }

  kfree(entries);
  return 0x0fffffff;
}

uint32_t fat_write_cluster(size_t part, uint8_t *buffer, uint32_t cluster) {
  sata_write(fat_parts.data[part].drive,
            fat_cluster_to_sector(part, cluster) +
                fat_parts.data[part].sector_start,
            fat_parts.data[part].boot.cluster_size, buffer);

  return fat_get_next_cluster(part, cluster);
}

void fat_write_cluster_chain(size_t part, uint8_t *buffer, uint32_t cluster) {
  while (cluster >= 2 && cluster < 0xffffff7) {
    cluster = fat_write_cluster(part, buffer, cluster);
    buffer += fat_parts.data[part].boot.cluster_size * 512;
  }
}

void fat_set_dir_entry(size_t part, uint32_t directory, size_t index, dir_entry_t entry) {
  fat_partition_t partition = fat_parts.data[part];

  if (!directory)
    directory = partition.boot.root_cluster;

  size_t size = fat_chain_cluster_length(part, directory) * partition.boot.cluster_size * 512;

  if (index > size / sizeof(dir_entry_t))
    return;

  dir_entry_t *entries = kmalloc(size);
  fat_read_cluster_chain(part, (uint8_t *)entries, directory);

  entries[index] = entry;
  fat_write_cluster_chain(part, (uint8_t *)entries, directory);

  kfree(entries);
}

uint32_t fat_find(size_t part, uint32_t directory, dir_entry_t *parent_cluster,
                  char *path) {
  fat_partition_t partition = fat_parts.data[part];

  if (directory >= 0xffffff7)
    return 0xfffffff;
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

  size_t size = fat_chain_cluster_length(part, directory) *
                partition.boot.cluster_size * 512;
  dir_entry_t *buffer = kmalloc(size);
  fat_read_cluster_chain(part, (uint8_t *)buffer, directory);

  size /= sizeof(dir_entry_t);

  for (size_t i = 0; i < size; i++) {
    dir_entry_t *cluster = &buffer[i];

    if (!cluster->filename[0] || cluster->filename[0] == 0xE5)
      continue;

    if (cluster->flags == 0xf) {
      long_filename_t *lfn = (void *)&buffer[i];
      size_t count = 0;

      while (lfn->flags != 0x41) {
        count++;
        buffer += sizeof(long_filename_t);
        lfn = (void *)buffer;
      }

      buffer -= sizeof(long_filename_t);
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

        buffer -= sizeof(long_filename_t);
        lfn = (void *)buffer;
      }

      i += count + 2;
      buffer += (count + 1) * 32;
      dir_entry_t *cluster = (void *)buffer;

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

      buffer -= sizeof(long_filename_t);

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

  kfree(buffer);
  return 0x0fffffff;
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

  return 0;
}
