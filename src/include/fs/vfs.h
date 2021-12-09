#ifndef __VFS_H__
#define __VFS_H__

#include <drivers/rtc.h>
#include <stddef.h>
#include <stdint.h>
#include <vec.h>

#define VFS_NULL 0
#define VFS_FILE 1
#define VFS_DIR 2

#define VFS_LONG_FILE_NAME 0b1
#define VFS_HIDDEN 0b10
#define VFS_SYSTEM 0b100
#define VFS_READ_ONLY 0b1000

typedef struct fs_file {
  char name[256];
  int is_valid;
  uint32_t flags;
  uint8_t type;
  size_t file_size;

  datetime_t created_time;
  datetime_t modified_time;
  datetime_t accessed_time;
} fs_file_t;

typedef vec_t(fs_file_t) vec_fs_file_t;

typedef struct fs_mountpoint {
  char type[16];
  int is_root_partition;
  size_t position_in_list;
  char drive_num;

  vec_fs_file_t (*list_directory)(size_t part, char *path);
  fs_file_t (*info)(size_t part, char *path);
  uint8_t (*create_file)(size_t part, char *path);
  uint8_t (*mkdir)(size_t part, char *path);
  uint8_t (*read)(size_t part, char *path, size_t offset, uint8_t *buffer,
                  size_t count);
  uint8_t (*write)(size_t part, char *path, size_t offset, uint8_t *buffer,
                   size_t count);
  uint8_t (*delete)(size_t part, char *path);
  uint8_t (*set_flags)(size_t part, char *path, uint32_t flags);
  size_t (*sizeof_file)(size_t part, char *path);
  int (*identify)(size_t part, char *path);

  void *fs_info;
} fs_mountpoint_t;

uint8_t vfs_read(char *path, size_t offset, size_t count, uint8_t *buffer);
uint8_t vfs_write(char *path, size_t offset, size_t count, uint8_t *buffer);
size_t vfs_sizeof_file(char *path);
uint8_t vfs_identify(char *path);
uint8_t vfs_delete(char *path);
uint8_t vfs_create_file(char *path);
uint8_t vfs_create_dir(char *path);
vec_fs_file_t vfs_list_dir(char *path);
fs_file_t vfs_get_info(char *path);
int init_vfs();

#endif
