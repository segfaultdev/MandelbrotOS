#ifndef __VFS_H__
#define __VFS_H__

#include <device.h>
#include <drivers/rtc.h>
#include <stddef.h>
#include <stdint.h>
#include <vec.h>

#define S_IFSOCK 0140000
#define S_IFLNK 0120000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFIFO 0010000
#define S_IFREG 0100000

#define SEEK_SET 0
#define SEEK_REL 1

#define OP_RDONLY 0x0
#define OP_WRONLY 0x1
#define OP_RDWR 0x2
#define OP_APPEND 0x8
#define OP_CREAT 0x200
#define OP_TRUNC 0x400
#define OP_EXCL 0x800
#define OP_SYNC 0x2000
#define OP_NOCTTY 0x8000
#define OP_CLOEXEC 0x40000

#define FILE_TYPE_NORMAL 0

typedef struct stat {
  uint16_t device_id;
  uint16_t file_serial;
  uint32_t file_mode;
  uint16_t hardlink_count;
  uint16_t user_id;
  uint16_t group_id;
  uint16_t device_type;
  uint32_t file_size;

  posix_time_t last_access_time;
  posix_time_t last_modification_time;
  posix_time_t last_status_change_time;
  posix_time_t creation_time;

  size_t block_size;
  size_t block_count;
} stat_t;

typedef struct dirent {
  char *name;
  uint16_t file_serial;
} dirent_t;

// This has to have symlinks added but I'll do that a bit later

typedef struct fs_ops {
  char *fs_name;
  int (*post_mount)(device_t *dev);
  struct fs *(*mount)(device_t *dev);
  struct fs_file *(*mkdir)(struct fs *fs, char *path, int mode);
  struct fs_file *(*create)(struct fs *fs, char *path);
  struct fs_file *(*open)(struct fs *fs, char *name);
} fs_ops_t;

typedef struct file_ops {
  int (*read)(struct fs_file *file, uint8_t *buf, size_t offset, size_t count);
  int (*write)(struct fs_file *file, uint8_t *buf, size_t offset, size_t count);
  int (*rmdir)(struct fs_file *file);
  int (*delete)(struct fs_file *file);
  int (*truncate)(struct fs_file *file, size_t size);
  int (*fstat)(struct fs_file *file, stat_t *stat);
  int (*close)(struct fs_file *file);
  int (*readdir)(struct fs_file *file, dirent_t *dirent);
  int (*ioctl)(struct fs_file *file, uint64_t cmd, uint64_t arg);
  int (*chmod)(struct fs_file *file, int mode);
  int (*chown)(struct fs_file *file, int uid, int gid);
} file_ops_t;

typedef struct fs {
  fs_ops_t *ops;
  device_t *dev;
  void *private_data;
} fs_t;

typedef struct fs_file {
  char *path;

  int uid;
  int gid;

  file_ops_t *file_ops;

  fs_t *fs;

  posix_time_t last_access_time;
  posix_time_t last_modification_time;
  posix_time_t last_status_change_time;

  size_t offset;
  int mode;
  int isdir;
  size_t length;
  int type;
  void *private_data;
} fs_file_t;

typedef struct vfs_mount_info {
  char *path;
  device_t *dev;
  fs_t *fs;
} vfs_mount_info_t;

void vfs_register_fs(fs_ops_t *ops);
int vfs_mount(char *path, device_t *dev);
int vfs_read(fs_file_t *file, uint8_t *buf, size_t offset, size_t count);
int vfs_write(fs_file_t *file, uint8_t *buf, size_t offset, size_t count);
int vfs_rmdir(fs_file_t *file);
int vfs_delete(fs_file_t *file);
int vfs_truncate(fs_file_t *file, size_t size);
int vfs_fstat(fs_file_t *file, stat_t *stat);
int vfs_close(fs_file_t *file);
int vfs_readdir(fs_file_t *file, dirent_t *dirent);
int vfs_ioctl(fs_file_t *file, uint64_t cmd, uint64_t arg);
int vfs_chmod(fs_file_t *file, int mode);
int vfs_chown(fs_file_t *file, int uid, int gid);
fs_file_t *vfs_mkdir(char *path, int mode);
fs_file_t *vfs_create(char *path);
fs_file_t *vfs_open(char *name);

int init_vfs();

#endif
