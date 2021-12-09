#include <drivers/mbr.h>
#include <drivers/rtc.h>
#include <fb/fb.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <klog.h>
#include <mm/pmm.h>
#include <printf.h>

vec_fat_partition_t fat_parts;

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
  for (size_t i = 0; i < fat_parts.data[part].boot.sectors_per_fat; i++) {
    uint32_t buffer[128];

    sata_read(fat_parts.data[part].drive,
              i + fat_parts.data[part].sector_start +
                  fat_parts.data[part].boot.reserved_sectors,
              1, (uint8_t *)buffer);

    for (uint8_t j = 0; j < 128; j++)
      if (!buffer[j]) {
        return ((i << 7) | j) & 0xfffffff;
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

    fat_change_fat_value(part, current, 0xffffff8);

    if (!start)
      start = current;

    previous = current;
  }

  return start;
}

void fat_free_chain(size_t part, uint32_t cluster) {
  while (1) {
    uint32_t next = fat_get_next_cluster(part, cluster);

    if (!next || next == 0xffffff8)
      return;

    fat_change_fat_value(part, cluster, 0);

    cluster = next;
  }
}

uint32_t fat_get_free_dir_entry_chain(size_t part, uint32_t directory,
                                      size_t length) {
  fat_partition_t partition = fat_parts.data[part];

  if (!directory)
    directory = partition.boot.root_cluster;

  size_t size = fat_chain_cluster_length(part, directory) *
                partition.boot.cluster_size * 512;
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
      } else if (!start)
        start = i;
    } else {
      found_length = 0;
      start = 0;
    }
  }

  kfree(entries);

  return 0xfffffff;
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

void fat_set_dir_entry(size_t part, uint32_t directory, size_t index,
                       dir_entry_t entry) {
  fat_partition_t partition = fat_parts.data[part];

  if (!directory)
    directory = partition.boot.root_cluster;

  size_t size = fat_chain_cluster_length(part, directory) *
                partition.boot.cluster_size * 512;

  if (index > size / sizeof(dir_entry_t))
    return;

  dir_entry_t *entries = kmalloc(size);
  fat_read_cluster_chain(part, (uint8_t *)entries, directory);

  entries[index] = entry;
  fat_write_cluster_chain(part, (uint8_t *)entries, directory);

  kfree(entries);
}

uint8_t fat_get_short_filename_checksum(uint8_t *name) {
  int i;
  uint8_t sum = 0;

  for (i = 11; i; i--)
    sum = ((sum & 1) << 7) + (sum >> 1) + *name++;
  return sum;
}

uint32_t fat_find(size_t part, uint32_t directory, dir_entry_t *parent_cluster,
                  uint32_t *parent_cluster_dir, size_t *index, char *path) {
  fat_partition_t partition = fat_parts.data[part];

  if (directory >= 0xffffff7)
    return 0xfffffff;
  if (!directory)
    directory = partition.boot.root_cluster;
  if (parent_cluster_dir)
    *parent_cluster_dir = directory;
  if (!path)
    return directory;
  if (!strlen(path))
    return directory;

  if (path[0] == ' ' || path[0] == '/')
    return fat_find(part, directory, parent_cluster, parent_cluster_dir, index,
                    ++path);

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

      while (lfn->flags != 0x1 && lfn->flags != 0x41) {
        count++;
        lfn = (void *)&buffer[i + count];
      }

      count++;

      lfn = (void *)&buffer[i + count - 1];

      uint8_t *long_filename = kmalloc(
          count *
          13); // Each long file name can hold 13 characters (If you convert
               // it from normal UTF16 to ASCII because nobody likes UTF16)

      memset(long_filename, 0xff, count * 13);

      for (size_t j = 0; j < count; j++) {
        for (size_t x = 0; x < 5; x++)
          long_filename[j * 13 + x] = lfn->filename_lo[x];
        for (size_t x = 0; x < 6; x++)
          long_filename[j * 13 + x + 5] = lfn->filename_mid[x];
        for (size_t x = 0; x < 2; x++)
          long_filename[j * 13 + x + 11] = lfn->filename_hi[x];

        lfn = (void *)&buffer[i + count - j - 2];
      }

      i += count;

      uint8_t *path_name = kmalloc(256);

      for (size_t i = 0; i < count * 13; i++)
        path_name[i] = 0xff;

      path -= pos;

      pos = 0;
      while (*path) {
        if (*path == '/')
          break;

        path_name[pos++] = *path;

        path++;
      }

      path_name[pos] = 0;

      i--;

      if (!strncmp((char *)path_name, (char *)long_filename, count * 13)) {
        dir_entry_t *cluster = &buffer[i + 1];

        uint32_t cluster_no = ((uint32_t)(cluster->cluster_hi) << 16) |
                              (uint32_t)(cluster->cluster_lo);

        if (*path == '/' && cluster->directory) {
          kfree(path_name);
          kfree(long_filename);
          kfree(buffer);

          cluster_no = fat_find(part, cluster_no, parent_cluster,
                                parent_cluster_dir, index, path + 1);
        } else {
          kfree(path_name);
          kfree(long_filename);
          kfree(buffer);

          if (parent_cluster)
            *parent_cluster = *cluster;
          if (parent_cluster_dir)
            *parent_cluster_dir = directory;
          if (index)
            *index = i;
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
          cluster_no = fat_find(part, cluster_no, parent_cluster,
                                parent_cluster_dir, index, path + 1);
        } else {
          kfree(buffer);

          if (parent_cluster)
            *parent_cluster = *cluster;
          if (parent_cluster_dir)
            *parent_cluster_dir = directory;
          if (index)
            *index = i;
        }

        return cluster_no;
      }
    }
  }

  kfree(buffer);
  return 0xfffffff;
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

// This is only the functions I need to link the FAT driver to the VFS

uint8_t fat_create_file(size_t part, char *path) {
  datetime_t time = get_datetime();
  uint32_t parent_cluster;

  if (fat_find(part, 0, NULL, &parent_cluster, NULL, path) != 0xfffffff)
    return 1;

  path = strrchr(path, '/');
  if (!path)
    return 1;

  path++;

  int is_long_filename = 0;
  size_t count = 0;
  size_t name_length = 0;
  while (1) {
    if (path[count] == 0)
      break;
    if (path[count] == '.') {
      count++;
      continue;
    }
    if (name_length > 10) {
      is_long_filename = 1;
      break;
    }

    name_length++;
    count++;
  }

  if (is_long_filename) {
    char short_path[11] = "           ";
    uint8_t pos = 0;

    while (*path) {
      if (pos == 11)
        break;
      if (pos > 6) {
        short_path[6] = '~';
        short_path[7] = '0';
      }
      if (*path == '.') {
        if (pos < 8)
          pos = 8;
      } else if (*path >= 'a' && *path <= 'z')
        short_path[pos++] = (*path - 'a') + 'A';
      else
        short_path[pos++] = *path;

      path++;
    }

    char tmp_short_name[13] = {0};
    memcpy(tmp_short_name + 1, short_path, 11);
    tmp_short_name[0] = '/';

    while (fat_find(part, parent_cluster, NULL, NULL, NULL, tmp_short_name) !=
           0xfffffff)
      tmp_short_name[7]++;

    short_path[6] = tmp_short_name[7];

    path -= pos;

    size_t len = strlen(path) / 13 + 1;
    size_t index = fat_get_free_dir_entry_chain(part, parent_cluster, len);
    uint8_t placement = 1;
    long_filename_t lfn = {0};
    int done = 0;

    for (size_t i = 0; i < len; i++) {
      memset(&lfn, 0xff, 32);

      for (size_t j = 0; j < 5; j++) {
        if (!done) {
          if (*path == 0) {
            done = 1;
            lfn.filename_lo[j] = 0;
            continue;
          }
          lfn.filename_lo[j] = *path;
          path++;
        } else
          lfn.filename_lo[j] = 0xffff;
      }
      for (size_t j = 0; j < 6; j++) {
        if (!done) {
          if (*path == 0) {
            done = 1;
            lfn.filename_mid[j] = 0;
            continue;
          }
          lfn.filename_mid[j] = *path;
          path++;
        } else
          lfn.filename_mid[j] = 0xffff;
      }
      for (size_t j = 0; j < 2; j++) {
        if (!done) {
          if (*path == 0) {
            done = 1;
            lfn.filename_hi[j] = 0;
            continue;
          }
          lfn.filename_hi[j] = *path;
          path++;
        } else
          lfn.filename_hi[j] = 0xffff;
      }

      lfn.attribute = 0xf;
      lfn.flags = placement;
      lfn.zero = 0;
      lfn.long_entry = 0;
      lfn.checksum_of_short_name = 0;

      lfn.checksum_of_short_name =
          fat_get_short_filename_checksum((uint8_t *)short_path);

      fat_set_dir_entry(part, parent_cluster, index + len - 1 - i,
                        *(dir_entry_t *)(void *)&lfn);

      if (placement == len - 1)
        placement = ((placement + 1) | 0x40);
      else
        placement++;
    }

    dir_entry_t dir = {0};

    uint32_t new_writing_cluster = fat_find_free_cluster(part);
    fat_change_fat_value(part, new_writing_cluster, 0xffffff8);
    dir.cluster_lo = new_writing_cluster;
    dir.cluster_hi = new_writing_cluster >> 16;

    dir.accessed_day = time.day;
    dir.accessed_month = time.month;
    dir.accessed_year = time.year - 1980;

    dir.created_day = time.day;
    dir.created_month = time.month;
    dir.created_year = time.year - 1980;
    dir.created_hour = time.hours;
    dir.created_minute = time.minutes;
    dir.created_second = time.seconds / 2;
    dir.created_ticks = time.seconds % 2;

    dir.modified_day = time.day;
    dir.modified_month = time.month;
    dir.modified_year = time.year - 1980;
    dir.modified_hour = time.hours;
    dir.modified_minute = time.minutes;
    dir.modified_second = time.seconds / 2;

    memcpy(dir.filename, short_path, 11);
    fat_set_dir_entry(part, parent_cluster, index + len, dir);
  } else {
    char short_path[11] = "           ";
    uint8_t pos = 0;

    while (*path) {
      if (pos == 11)
        break;

      if (*path == '.') {
        if (pos < 8)
          pos = 8;
      } else if (*path >= 'a' && *path <= 'z')
        short_path[pos++] = (*path - 'a') + 'A';
      else
        short_path[pos++] = *path;

      path++;
    }

    size_t index = fat_get_free_dir_entry_chain(part, parent_cluster, 1);

    dir_entry_t dir = {0};

    uint32_t new_writing_cluster = fat_find_free_cluster(part);
    fat_change_fat_value(part, new_writing_cluster, 0xffffff8);
    dir.cluster_lo = new_writing_cluster;
    dir.cluster_hi = new_writing_cluster >> 16;

    dir.accessed_day = time.day;
    dir.accessed_month = time.month;
    dir.accessed_year = time.year - 1980;

    dir.created_day = time.day;
    dir.created_month = time.month;
    dir.created_year = time.year - 1980;
    dir.created_hour = time.hours;
    dir.created_minute = time.minutes;
    dir.created_second = time.seconds / 2;
    dir.created_ticks = time.seconds % 2;

    dir.modified_day = time.day;
    dir.modified_month = time.month;
    dir.modified_year = time.year - 1980;
    dir.modified_hour = time.hours;
    dir.modified_minute = time.minutes;
    dir.modified_second = time.seconds / 2;

    memcpy(dir.filename, (uint8_t *)short_path, 11);
    fat_set_dir_entry(part, parent_cluster, index, dir);
  }

  return 0;
}

uint8_t fat_create_directory(size_t part, char *path) {
  datetime_t time = get_datetime();

  uint32_t parent_cluster;

  if (fat_find(part, 0, NULL, &parent_cluster, NULL, path) != 0xfffffff)
    return 1;

  path = strrchr(path, '/');
  if (!path)
    return 1;

  path++;

  int is_long_filename = 0;
  size_t count = 0;
  size_t name_length = 0;
  while (1) {
    if (path[count] == 0)
      break;
    if (path[count] == '.') {
      count++;
      continue;
    }
    if (name_length > 10) {
      is_long_filename = 1;
      break;
    }

    name_length++;
    count++;
  }

  if (is_long_filename) {
    char short_path[11] = "           ";
    uint8_t pos = 0;

    while (*path) {
      if (pos == 11)
        break;
      if (pos > 6) {
        short_path[6] = '~';
        short_path[7] = '0';
      }
      if (*path >= 'a' && *path <= 'z')
        short_path[pos++] = (*path - 'a') + 'A';
      else
        short_path[pos++] = *path;

      path++;
    }

    char tmp_short_name[13] = {0};
    memcpy(tmp_short_name + 1, short_path, 11);
    tmp_short_name[0] = '/';

    while (fat_find(part, parent_cluster, NULL, NULL, NULL, tmp_short_name) !=
           0xfffffff)
      tmp_short_name[7]++;

    short_path[6] = tmp_short_name[7];

    path -= pos;

    size_t len = strlen(path) / 13 + 1;
    size_t index = fat_get_free_dir_entry_chain(part, parent_cluster, len);
    uint8_t placement = 1;
    long_filename_t lfn = {0};
    int done = 0;

    for (size_t i = 0; i < len; i++) {
      memset(&lfn, 0xff, 32);

      for (size_t j = 0; j < 5; j++) {
        if (!done) {
          if (*path == 0) {
            done = 1;
            lfn.filename_lo[j] = 0;
            continue;
          }
          lfn.filename_lo[j] = *path;
          path++;
        } else
          lfn.filename_lo[j] = 0xffff;
      }
      for (size_t j = 0; j < 6; j++) {
        if (!done) {
          if (*path == 0) {
            done = 1;
            lfn.filename_mid[j] = 0;
            continue;
          }
          lfn.filename_mid[j] = *path;
          path++;
        } else
          lfn.filename_mid[j] = 0xffff;
      }
      for (size_t j = 0; j < 2; j++) {
        if (!done) {
          if (*path == 0) {
            done = 1;
            lfn.filename_hi[j] = 0;
            continue;
          }
          lfn.filename_hi[j] = *path;
          path++;
        } else
          lfn.filename_hi[j] = 0xffff;
      }

      lfn.attribute = 0xf;
      lfn.flags = placement;
      lfn.zero = 0;
      lfn.long_entry = 0;
      lfn.checksum_of_short_name = 0;

      lfn.checksum_of_short_name =
          fat_get_short_filename_checksum((uint8_t *)short_path);

      fat_set_dir_entry(part, parent_cluster, index + len - 1 - i,
                        *(dir_entry_t *)(void *)&lfn);

      if (placement == len - 1)
        placement = ((placement + 1) | 0x40);
      else
        placement++;
    }

    dir_entry_t dir = {0};

    uint32_t new_writing_cluster = fat_find_free_cluster(part);
    fat_change_fat_value(part, new_writing_cluster, 0xffffff8);

    dir.accessed_day = time.day;
    dir.accessed_month = time.month;
    dir.accessed_year = time.year - 1980;

    dir.created_day = time.day;
    dir.created_month = time.month;
    dir.created_year = time.year - 1980;
    dir.created_hour = time.hours;
    dir.created_minute = time.minutes;
    dir.created_second = time.seconds / 2;
    dir.created_ticks = (time.seconds % 2) * 100;

    dir.modified_day = time.day;
    dir.modified_month = time.month;
    dir.modified_year = time.year - 1980;
    dir.modified_hour = time.hours;
    dir.modified_minute = time.minutes;
    dir.modified_second = time.seconds / 2;

    dir.cluster_lo = new_writing_cluster;
    dir.cluster_hi = new_writing_cluster >> 16;
    dir.directory = 1;

    memcpy(dir.filename, short_path, 11);
    fat_set_dir_entry(part, parent_cluster, index + len, dir);
  } else {
    char short_path[11] = "           ";
    uint8_t pos = 0;

    while (*path) {
      if (pos == 11)
        break;

      if (*path >= 'a' && *path <= 'z')
        short_path[pos++] = (*path - 'a') + 'A';
      else
        short_path[pos++] = *path;

      path++;
    }

    size_t index = fat_get_free_dir_entry_chain(part, parent_cluster, 1);

    dir_entry_t dir = {0};

    uint32_t new_writing_cluster = fat_find_free_cluster(part);
    fat_change_fat_value(part, new_writing_cluster, 0xffffff8);

    dir.accessed_day = time.day;
    dir.accessed_month = time.month;
    dir.accessed_year = time.year - 1980;

    dir.created_day = time.day;
    dir.created_month = time.month;
    dir.created_year = time.year - 1980;
    dir.created_hour = time.hours;
    dir.created_minute = time.minutes;
    dir.created_second = time.seconds / 2;
    dir.created_ticks = (time.seconds % 2) * 100;

    dir.modified_day = time.day;
    dir.modified_month = time.month;
    dir.modified_year = time.year - 1980;
    dir.modified_hour = time.hours;
    dir.modified_minute = time.minutes;
    dir.modified_second = time.seconds / 2;

    dir.cluster_lo = new_writing_cluster;
    dir.cluster_hi = new_writing_cluster >> 16;
    dir.directory = 1;

    dir.cluster_lo = new_writing_cluster;
    dir.cluster_hi = new_writing_cluster >> 16;
    dir.directory = 1;

    memcpy(dir.filename, (uint8_t *)short_path, 11);
    fat_set_dir_entry(part, parent_cluster, index, dir);
  }

  return 0;
}

uint8_t fat_read_file(size_t part, char *path, size_t offset, uint8_t *buffer,
                      size_t count) {
  datetime_t time = get_datetime();

  size_t index;
  uint32_t dir;

  dir_entry_t entry = {0};

  if (fat_find(part, 0, &entry, &dir, &index, path) == 0xfffffff)
    return 1;

  entry.accessed_day = time.day;
  entry.accessed_month = time.month;
  entry.accessed_year = time.year - 1980;

  fat_set_dir_entry(part, dir, index, entry);

  if (!count)
    return 1;
  if (entry.directory)
    return 1;
  if (entry.size < offset + count)
    return 1;

  uint8_t *file_buffer = kmalloc(entry.size);
  fat_read_cluster_chain(
      part, file_buffer,
      ((uint32_t)(entry.cluster_hi << 16) | (uint32_t)(entry.cluster_lo)));

  memcpy(buffer, file_buffer + offset, count);

  return 0;
}

uint8_t fat_write_file(size_t part, char *path, size_t offset, uint8_t *buffer,
                       size_t count) {
  datetime_t time = get_datetime();

  size_t index;
  uint32_t dir;

  dir_entry_t entry = {0};

  if (fat_find(part, 0, &entry, &dir, &index, path) == 0xfffffff)
    return 1;

  if (entry.read_only)
    return 1;

  entry.accessed_day = time.day;
  entry.accessed_month = time.month;
  entry.accessed_year = time.year - 1980;

  if (!count)
    return 1;
  if (entry.directory)
    return 1;
  if (entry.size < offset + count)
    return 1;

  uint8_t *file_buffer = kmalloc(entry.size);
  fat_read_cluster_chain(
      part, file_buffer,
      ((uint32_t)(entry.cluster_hi << 16) | (uint32_t)(entry.cluster_lo)));

  memcpy(file_buffer + offset, buffer, count);

  fat_write_cluster_chain(
      part, file_buffer,
      ((uint32_t)(entry.cluster_hi << 16) | (uint32_t)(entry.cluster_lo)));

  entry.modified_day = time.day;
  entry.modified_month = time.month;
  entry.modified_year = time.year - 1980;
  entry.modified_hour = time.hours;
  entry.modified_minute = time.minutes;
  entry.modified_second = time.seconds / 2;

  fat_set_dir_entry(part, dir, index, entry);

  return 0;
}

size_t fat_sizeof_file(size_t part, char *path) {
  datetime_t time = get_datetime();

  size_t index;
  uint32_t dir;

  dir_entry_t entry = {0};

  if (fat_find(part, 0, &entry, &dir, &index, path) == 0xfffffff)
    return -1;

  entry.accessed_day = time.day;
  entry.accessed_month = time.month;
  entry.accessed_year = time.year - 1980;

  fat_set_dir_entry(part, dir, index, entry);

  return entry.size;
}

fs_file_t fat_info_to_vfs_info(size_t part, char *path) {
  fs_file_t file = {0};
  datetime_t time = get_datetime();

  size_t index;
  uint32_t dir;

  dir_entry_t entry = {0};

  if (fat_find(part, 0, &entry, &dir, &index, path) == 0xfffffff)
    return (fs_file_t){0};

  size_t size = fat_sizeof_file(part, path);

  path = strrchr(path, '/');
  if (!path)
    return (fs_file_t){0};
  path++;

  file = (fs_file_t){
      .file_size = size,
      .type = (entry.directory) ? VFS_DIR : VFS_FILE,
      .is_valid = 1,
  };

  strcpy(file.name, path);

  int is_long_filename = 0;
  size_t count = 0;
  size_t name_length = 0;
  while (1) {
    if (path[count] == 0)
      break;
    if (path[count] == '.') {
      count++;
      continue;
    }
    if (name_length > 10) {
      is_long_filename = 1;
      break;
    }

    name_length++;
    count++;
  }

  if (is_long_filename)
    file.flags |= VFS_LONG_FILE_NAME;
  if (entry.read_only)
    file.flags |= VFS_READ_ONLY;
  if (entry.hidden)
    file.flags |= VFS_HIDDEN;
  if (entry.system)
    file.flags |= VFS_SYSTEM;

  entry.accessed_day = time.day;
  entry.accessed_month = time.month;
  entry.accessed_year = time.year - 1980;

  fat_set_dir_entry(part, dir, index, entry);

  file.created_time = (datetime_t){
      .day = entry.created_day,
      .month = entry.created_month,
      .year = entry.created_year + 1980,
      .hours = entry.created_hour,
      .minutes = entry.created_minute,
      .seconds = entry.created_second * 2 + (entry.created_ticks / 100),
  };

  file.modified_time = (datetime_t){
      .day = entry.modified_day,
      .month = entry.modified_month,
      .year = entry.modified_year + 1980,
      .hours = entry.modified_hour,
      .minutes = entry.modified_minute,
      .seconds = entry.modified_second * 2,
  };

  file.accessed_time = (datetime_t){
      .day = entry.accessed_day,
      .month = entry.accessed_month,
      .year = entry.accessed_year + 1980,
  };

  return file;
}

vec_fs_file_t fat_list_directory(size_t part, char *path) {
  datetime_t time = get_datetime();
  uint32_t current_dir;

  uint32_t cluster = fat_find(part, 0, NULL, &current_dir, NULL, path);

  vec_fs_file_t files = {0};

  if (!cluster)
    return files;

  size_t size = fat_chain_cluster_length(part, cluster) * 512;
  dir_entry_t *entries = kmalloc(size);

  size /= sizeof(dir_entry_t);
  files.data = kmalloc(sizeof(fs_file_t));

  fat_read_cluster_chain(0, (uint8_t *)entries, cluster);

  for (size_t i = 0; i < size; i++) {
    if (!entries[i].filename[0] || entries[i].filename[0] == 0xe5)
      continue;

    if (entries[i].flags == 0xf) {
      long_filename_t *lfn = (void *)&entries[i];
      size_t count = 0;

      while (lfn->flags != 0x1 && lfn->flags != 0x41) {
        count++;
        lfn = (void *)&entries[i + count];
      }

      count++;

      lfn = (void *)&entries[i + count - 1];

      uint8_t *long_filename = kmalloc(
          count *
          13); // Each long file name can hold 13 characters (If you convert
               // it from normal UTF16 to ASCII because nobody likes UTF16);

      for (size_t j = 0; j < count; j++) {
        for (size_t x = 0; x < 5; x++)
          long_filename[j * 13 + x] = lfn->filename_lo[x];
        for (size_t x = 0; x < 6; x++)
          long_filename[j * 13 + x + 5] = lfn->filename_mid[x];
        for (size_t x = 0; x < 2; x++)
          long_filename[j * 13 + x + 11] = lfn->filename_hi[x];

        lfn = (void *)&entries[i + count - j - 2];
      }

      fs_file_t new_file = (fs_file_t){
          .type = VFS_FILE,
          .file_size = entries[i + 1].size,
          .is_valid = 1,
      };

      new_file.flags |= VFS_LONG_FILE_NAME;
      if (entries[i + 1].read_only)
        new_file.flags |= VFS_READ_ONLY;
      if (entries[i + 1].hidden)
        new_file.flags |= VFS_HIDDEN;
      if (entries[i + 1].system)
        new_file.flags |= VFS_SYSTEM;

      new_file.created_time = (datetime_t){
          .day = entries[i].created_day,
          .month = entries[i].created_month,
          .year = entries[i].created_year + 1980,
          .hours = entries[i].created_hour,
          .minutes = entries[i].created_minute,
          .seconds =
              entries[i].created_second * 2 + (entries[i].created_ticks / 100),
      };

      new_file.modified_time = (datetime_t){
          .day = entries[i].modified_day,
          .month = entries[i].modified_month,
          .year = entries[i].modified_year + 1980,
          .hours = entries[i].modified_hour,
          .minutes = entries[i].modified_minute,
          .seconds = entries[i].modified_second * 2,
      };

      new_file.accessed_time = (datetime_t){
          .day = entries[i].accessed_day,
          .month = entries[i].accessed_month,
          .year = entries[i].accessed_year + 1980,
      };

      strcat(new_file.name, (char *)long_filename);
      vec_push(&files, new_file);

      i += count - 1;

      kfree(long_filename);

      continue;
    }

    fs_file_t new_file = (fs_file_t){
        .type = VFS_FILE,
        .file_size = entries[i + 1].size,
        .is_valid = 1,
    };

    if (entries[i].read_only)
      new_file.flags |= VFS_READ_ONLY;
    if (entries[i].hidden)
      new_file.flags |= VFS_HIDDEN;
    if (entries[i].system)
      new_file.flags |= VFS_SYSTEM;

    new_file.created_time = (datetime_t){
        .day = entries[i].created_day,
        .month = entries[i].created_month,
        .year = entries[i].created_year + 1980,
        .hours = entries[i].created_hour,
        .minutes = entries[i].created_minute,
        .seconds =
            entries[i].created_second * 2 + (entries[i].created_ticks / 100),
    };

    new_file.modified_time = (datetime_t){
        .day = entries[i].modified_day,
        .month = entries[i].modified_month,
        .year = entries[i].modified_year + 1980,
        .hours = entries[i].modified_hour,
        .minutes = entries[i].modified_minute,
        .seconds = entries[i].modified_second * 2,
    };

    new_file.accessed_time = (datetime_t){
        .day = entries[i].accessed_day,
        .month = entries[i].accessed_month,
        .year = entries[i].accessed_year + 1980,
    };

    entries[i].accessed_day = time.day;
    entries[i].accessed_month = time.month;
    entries[i].accessed_year = time.year - 1980;

    fat_set_dir_entry(part, current_dir,
                      (i + 1) % (fat_parts.data[part].boot.cluster_size * 512 /
                                 sizeof(dir_entry_t)),
                      entries[i]);

    if ((i + 1) * sizeof(dir_entry_t) %
            fat_parts.data[part].boot.cluster_size ==
        0)
      current_dir = fat_get_next_cluster(part, current_dir);

    strcat(new_file.name, (char *)entries[i].filename);
    new_file.name[11] = 0;
    vec_push(&files, new_file);
  }

  return files;
}

uint8_t fat_delete(size_t part, char *path) {
  dir_entry_t entry = {0};
  size_t index;
  uint32_t dir;
  uint32_t cluster = fat_find(part, 0, &entry, &dir, &index, path);

  if (cluster == 0xfffffff)
    return 1;

  if (entry.directory) {
    vec_fs_file_t nodes = fat_list_directory(part, path);

    for (size_t i = 0; i < (size_t)nodes.length; i++) {
      if (nodes.data[i].name[0] == '.')
        continue;

      char *new_path = kmalloc(strlen(path) + strlen(nodes.data[i].name));
      strcpy(new_path, path);
      strcat(new_path, "/");
      strcat(new_path, nodes.data[i].name);

      fat_delete(part, new_path);

      kfree(new_path);
    }
  } else {
    fat_free_chain(part, cluster);
    entry.filename[0] = 0xe5;
    fat_set_dir_entry(part, dir, index, entry);
  }

  return 0;
}

int fat_identify_file_or_dir(size_t part, char *path) {
  datetime_t time = get_datetime();

  uint32_t directory;
  size_t index;
  dir_entry_t dir = {0};

  if (fat_find(part, 0, &dir, &directory, &index, path) == 0xfffffff)
    return VFS_NULL;

  dir.accessed_day = time.day;
  dir.accessed_month = time.month;
  dir.accessed_year = time.year - 1980;

  fat_set_dir_entry(part, directory, index, dir);

  if (dir.directory)
    return VFS_DIR;
  return VFS_FILE;
}

uint8_t fat_set_flags(size_t part, char *path, uint32_t flags) {
  datetime_t time = get_datetime();

  uint32_t directory;
  size_t index;
  dir_entry_t dir = {0};

  if (fat_find(part, 0, &dir, &directory, &index, path) == 0xfffffff)
    return 1;

  dir.accessed_day = time.day;
  dir.accessed_month = time.month;
  dir.accessed_year = time.year - 1980;

  if (flags & VFS_READ_ONLY)
    dir.read_only = 1;
  if (flags & VFS_SYSTEM)
    dir.system = 1;
  if (flags & VFS_HIDDEN)
    dir.hidden = 1;

  fat_set_dir_entry(part, directory, index, dir);

  return 0;
}

fs_mountpoint_t fat_partition_to_fs_node(size_t part) {
  fs_mountpoint_t new_node = (fs_mountpoint_t){
      .list_directory =
          (vec_fs_file_t(*)(size_t part, char *path))fat_list_directory,
      .create_file = (uint8_t(*)(size_t part, char *path))fat_create_file,
      .mkdir = (uint8_t(*)(size_t part, char *path))fat_create_directory,
      .read = (uint8_t(*)(size_t part, char *path, size_t offset,
                          uint8_t *buffer, size_t count))fat_read_file,
      .write = (uint8_t(*)(size_t part, char *path, size_t offset,
                           uint8_t *buffer, size_t count))fat_write_file,
      .identify = (int (*)(size_t part, char *path))fat_identify_file_or_dir,
      .sizeof_file = (size_t(*)(size_t part, char *path))fat_sizeof_file,
      .delete = (uint8_t(*)(size_t part, char *path))fat_delete,
      .set_flags = (uint8_t(*)(size_t part, char *path, uint32_t flags))fat_set_flags,
      .info = (fs_file_t(*)(size_t part, char *path))fat_info_to_vfs_info,
      .fs_info = (void *)&fat_parts.data[part],
  };

  uint8_t boot_buffer[512] = {0};
  sata_read(fat_parts.data[part].drive, 0, 1, boot_buffer);
  if (!memcmp((void *)boot_buffer, (void *)(0x7c00 + PHYS_MEM_OFFSET),
              0x15a)) { // For some reason 0x7c00 + 0x15b != boot_buffer[0x15b]
                        // even though they should be the same
    klog(3, "Found root disk on disk %zu\r\n", fat_parts.data[part].drive);
    new_node.is_root_partition = 1;
  }

  return new_node;
}
