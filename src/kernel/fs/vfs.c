#include <dev/device.h>
#include <drivers/ahci.h>
#include <fs/devfs.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <klog.h>
#include <mm/kheap.h>
#include <mm/vmm.h>
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

fs_t *vfs_mount_fs(device_t *dev, char *name) {
  for (size_t i = 0; i < (size_t)registered_fses.length; i++)
    if (registered_fses.data[i]->mount) {
      if (name) {
        if (!strcmp(name, registered_fses.data[i]->fs_name)) {
          fs_t *fs = registered_fses.data[i]->mount(dev);
          if (fs)
            return fs;
        }
      } else {
        fs_t *fs = registered_fses.data[i]->mount(dev);
        if (fs)
          return fs;
      }
    }

  return NULL;
}

int vfs_mount(char *path, device_t *dev, char *fs_name) {
  if (!root_mount && strcmp(path, "/"))
    return 1;
  if (vfs_is_mounted(path))
    return 1;

  fs_t *fs = vfs_mount_fs(dev, fs_name);
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

  if (dev)
    dev->fs = fs;

  if (dev)
    klog(3, "Mounted %s filesystem on device %lu on %s\r\n", fs_name, dev->id,
         path);
  else
    klog(3, "Mounted %s filesystem on %s\r\n", fs_name, path);

  return 0;
}

void vfs_register_fs(fs_ops_t *ops) { vec_push(&registered_fses, ops); }

ssize_t vfs_read(fs_file_t *file, uint8_t *buf, size_t offset, size_t count) {
  if (ISDEV(file))
    return device_read(file->dev, offset, count, buf);
  else
    return file->file_ops->read(file, buf, offset, count);
}

ssize_t vfs_write(fs_file_t *file, uint8_t *buf, size_t offset, size_t count) {
  if (ISDEV(file))
    return device_write(file->dev, offset, count, buf);
  else
    return file->file_ops->write(file, buf, offset, count);
}

void *vfs_mmap(struct fs_file *file, pagemap_t *pg, syscall_file_t *sfile,
               void *addr, size_t size, size_t offset, int prot, int flags) {
  if (ISDEV(file))
    return file->dev->mmap(file->dev, pg, sfile, addr, size, offset, prot,
                           flags);
  else
    return file->file_ops->mmap(file, pg, sfile, addr, size, offset, prot,
                                flags);
}

int vfs_rmdir(fs_file_t *file) { return file->file_ops->rmdir(file); }

int vfs_delete(fs_file_t *file) { return file->file_ops->delete (file); }

int vfs_truncate(fs_file_t *file, size_t size) {
  return file->file_ops->truncate(file, size);
}

int vfs_fstat(fs_file_t *file, stat_t *stat) {
  *stat = (stat_t){
      .device_id = (file->fs->dev) ? file->fs->dev->id : (uint64_t)-1,
      .device_type = (file->fs->dev) ? file->fs->dev->type : (uint64_t)-1,
      .file_serial = file->inode,
      .hardlink_count = 0,
      .user_id = file->uid,
      .group_id = file->gid,
      .file_size = file->length,

      .last_access_time = file->last_access_time,
      .last_modification_time = file->last_modification_time,
      .last_status_change_time = file->last_status_change_time,
      .creation_time = file->creation_time,

      .block_count = (file->dev) ? file->dev->block_count : 0,
      .block_size = (file->dev) ? file->dev->block_size : 0,
  };

  return 0;
}

int vfs_close(fs_file_t *file) { return file->file_ops->close(file); }

int vfs_readdir(fs_file_t *file, dirent_t *dirent, size_t pos) {
  return file->file_ops->readdir(file, dirent, pos);
}

uint64_t vfs_ioctl(fs_file_t *file, uint64_t cmd, void *arg) {
  if (ISDEV(file))
    return file->dev->ioctl(file->dev, cmd, arg);
  else
    return file->file_ops->ioctl(file, cmd, arg);
}

int vfs_chmod(fs_file_t *file, int mode) {
  return file->file_ops->chmod(file, mode);
}

int vfs_chown(fs_file_t *file, int uid, int gid) {
  return file->file_ops->chown(file, uid, gid);
}

fs_file_t *vfs_mknod(char *path, int mode, int uid, int gid, device_t *dev) {
  vfs_mount_info_t *mi = vfs_find_mount(path);
  char *str =
      (strlen(path) == strlen(mi->path)) ? "/" : path + strlen(mi->path) - 1;
  return mi->fs->ops->mknod(mi->fs, str, mode, uid, gid, dev);
}

fs_file_t *vfs_mkdir(char *path, int mode, int uid, int gid) {
  vfs_mount_info_t *mi = vfs_find_mount(path);
  char *str =
      (strlen(path) == strlen(mi->path)) ? "/" : path + strlen(mi->path) - 1;
  return mi->fs->ops->mkdir(mi->fs, str, mode, uid, gid);
}

fs_file_t *vfs_create(char *path, int mode, int uid, int gid) {
  vfs_mount_info_t *mi = vfs_find_mount(path);
  char *str =
      (strlen(path) == strlen(mi->path)) ? "/" : path + strlen(mi->path) - 1;
  return mi->fs->ops->create(mi->fs, str, mode, uid, gid);
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
