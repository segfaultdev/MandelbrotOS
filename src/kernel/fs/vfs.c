#include <device.h>
#include <drivers/ahci.h>
#include <fs/devfs.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <klog.h>
#include <mm/kheap.h>
#include <printf.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vec.h>

vec_t(vfs_mount_info_t *) vfs_mounts = {0};
vec_t(fs_ops_t *) registered_fses = {0};

vfs_mount_info_t *root_mount;

static inline int vfs_is_mounted(char *path) {
  for (size_t i = 0; i < (size_t)vfs_mounts.length; i++)
    if (!strcmp(vfs_mounts.data[i]->path, path))
      return 1;
  return 0;
}

void vfs_seek(fs_file_t *file, int64_t count, int mode) {
  switch (mode) {
    case SEEK_SET:
      file->offset = (size_t)count;
      break;
    case SEEK_REL:
      file->offset += count;
      break;
  }
}

vfs_mount_info_t *vfs_find_mount(char *path) {
  char *path_dup = kcalloc(strlen(path) + 1);
  strcpy(path_dup, path);

  if (!strcmp(path_dup, "/")) {
    kfree(path_dup);
    return root_mount;
  }

  while (strlen(path_dup) > 1) {
    for (size_t i = 0; i < (size_t)vfs_mounts.length; i++)
      if (!strcmp(vfs_mounts.data[i]->path, path_dup)) {
        kfree(path_dup);
        return vfs_mounts.data[i];
      }
    path_dup[strlen(path_dup) - 1] = 0;
  }

  kfree(path_dup);
  return root_mount;
}

fs_t *vfs_mount_fs(device_t *dev) {
  for (size_t i = 0; i < (size_t)registered_fses.length; i++)
    if (registered_fses.data[i]->mount) {
      fs_t *fs = registered_fses.data[i]->mount(dev);
      if (fs)
        return fs;
    }

  return NULL;
}

int vfs_mount(char *path, device_t *dev) {
  if (!root_mount && strcmp(path, "/"))
    return 1;
  if (vfs_is_mounted(path))
    return 1;

  fs_t *fs = vfs_mount_fs(dev);
  if (!fs)
    return 1;

  vfs_mount_info_t *mi = kmalloc(sizeof(vfs_mount_info_t));

  *mi = (vfs_mount_info_t){
      .dev = dev,
      .path = path,
      .fs = fs,
  };

  vec_push(&vfs_mounts, mi);

  if (!root_mount)
    root_mount = mi;

  dev->fs = fs;

  return 0;
}

void vfs_register_fs(fs_ops_t *ops) { vec_push(&registered_fses, ops); }

int vfs_read(fs_file_t *file, uint8_t *buf, size_t offset, size_t count) {
  return file->file_ops->read(file, buf, offset, count);
}

int vfs_write(fs_file_t *file, uint8_t *buf, size_t offset, size_t count) {
  return file->file_ops->write(file, buf, offset, count);
}

int vfs_rmdir(fs_file_t *file) { return file->file_ops->rmdir(file); }

int vfs_delete(fs_file_t *file) { return file->file_ops->delete (file); }

int vfs_truncate(fs_file_t *file, size_t size) {
  return file->file_ops->truncate(file, size);
}

int vfs_fstat(fs_file_t *file, stat_t *stat) {
  return file->file_ops->fstat(file, stat);
}

int vfs_close(fs_file_t *file) { return file->file_ops->close(file); }

int vfs_readdir(fs_file_t *file, dirent_t *dirent) {
  return file->file_ops->readdir(file, dirent);
}

int vfs_ioctl(fs_file_t *file, uint64_t cmd, uint64_t arg) {
  return file->file_ops->ioctl(file, cmd, arg);
}

int vfs_chmod(fs_file_t *file, int mode) {
  return file->file_ops->chmod(file, mode);
}

int vfs_chown(fs_file_t *file, int uid, int gid) {
  return file->file_ops->chown(file, uid, gid);
}

fs_file_t *vfs_mkdir(char *path, int mode) {
  vfs_mount_info_t *mi = vfs_find_mount(path);
  char *str =
      (strlen(path) == strlen(mi->path)) ? "/" : path + strlen(mi->path) - 1;
  return mi->fs->ops->mkdir(mi->fs, str, mode);
}

fs_file_t *vfs_create(char *path) {
  vfs_mount_info_t *mi = vfs_find_mount(path);
  char *str =
      (strlen(path) == strlen(mi->path)) ? "/" : path + strlen(mi->path) - 1;
  return mi->fs->ops->create(mi->fs, str);
}

fs_file_t *vfs_open(char *name) {
  vfs_mount_info_t *mi = vfs_find_mount(name);
  char *str =
      (strlen(name) == strlen(mi->path)) ? "/" : name + strlen(mi->path) - 1;
  return mi->fs->ops->open(mi->fs, str);
}

int init_vfs() {
  vfs_mounts.data = kmalloc(sizeof(vfs_mount_info_t));
  registered_fses.data = kmalloc(sizeof(fs_ops_t));

  return 0;
}
