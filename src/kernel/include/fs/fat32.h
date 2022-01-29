#ifndef __FAT32_H__
#define __FAT32_H__

#include <fs/vfs.h>
#include <stddef.h>
#include <stdint.h>
#include <vec.h>

typedef struct bpb {
  uint8_t jump_bytes[3];
  uint8_t oem_ident[8];

  uint16_t sector_size;
  uint8_t cluster_size;
  uint16_t reserved_sectors;

  uint8_t table_count;
  uint16_t root_entries;

  uint16_t small_sector_count;

  uint8_t media_type;

  uint16_t legacy_table_size;
  uint16_t sectors_per_track;
  uint16_t head_count;

  uint32_t hidden_sectors;
  uint32_t large_sector_count;

  uint32_t table_size;

  uint16_t flags;

  uint16_t fat_version;

  uint32_t root_cluster;
  uint16_t fs_info_sector;
  uint16_t backup_bootsector_sector;

  uint8_t reserved1[12];

  uint8_t drive_number;

  uint8_t reserved2;

  uint8_t signature;
  uint32_t serial_no;
  uint8_t volume_label[11];
  uint8_t identifier[8];

  uint8_t boot_code[420];
  uint16_t bootable_partition_signature;
} __attribute__((packed)) bpb_t;

typedef struct dir_entry {
  uint8_t filename[11];

  union {
    struct {
      uint8_t read_only : 1;
      uint8_t hidden : 1;
      uint8_t system : 1;
      uint8_t volume_id : 1;
      uint8_t directory : 1;
      uint8_t archive : 1;
      uint8_t reserved1 : 2;
    };
    uint8_t flags;
  };

  uint8_t reserved2;

  uint8_t created_ticks;
  uint16_t created_second : 5;
  uint16_t created_minute : 6;
  uint16_t created_hour : 5;
  uint16_t created_day : 5;
  uint16_t created_month : 4;
  uint16_t created_year : 7;

  uint16_t accessed_day : 5;
  uint16_t accessed_month : 4;
  uint16_t accessed_year : 7;

  uint16_t cluster_hi;

  uint16_t modified_second : 5;
  uint16_t modified_minute : 6;
  uint16_t modified_hour : 5;

  uint16_t modified_day : 5;
  uint16_t modified_month : 4;
  uint16_t modified_year : 7;

  uint16_t cluster_lo;

  uint32_t size;
} __attribute__((packed)) dir_entry_t;

typedef struct long_filename {
  uint8_t flags;
  uint16_t filename_lo[5];
  uint8_t attribute;
  uint8_t long_entry;
  uint8_t checksum_of_short_name;
  uint16_t filename_mid[6];
  uint16_t zero;
  uint16_t filename_hi[2];
} __attribute__((packed)) long_filename_t;

typedef struct fat_fs_private_info {
  bpb_t boot;

  uint32_t sector_start;
  uint32_t sector_length;

  uint8_t partiton;
} fat_fs_private_info_t;

typedef struct fat_file_private_info {
  dir_entry_t dir;
  size_t index;
  uint32_t cluster;
  uint32_t parent_cluster;
} fat_file_private_info_t;

int init_fat();

#endif
