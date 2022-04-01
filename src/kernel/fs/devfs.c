#include <dev/device.h>
#include <fb/fb.h>
#include <fs/devfs.h>
#include <fs/tmpfs.h>
#include <fs/vfs.h>
#include <mm/kheap.h>
#include <printf.h>
#include <string.h>
#include <vec.h>

// ATM, devfs is the same as tmpfs

fs_ops_t devfs_fs_ops;
file_ops_t devfs_file_ops;

int init_devfs() {
  memcpy(&devfs_fs_ops, &tmpfs_fs_ops, sizeof(fs_ops_t));
  memcpy(&devfs_file_ops, &tmpfs_file_ops, sizeof(file_ops_t));

  devfs_fs_ops.fs_name = strdup("DEVFS");

  vfs_register_fs(&devfs_fs_ops);

  return 0;
}
