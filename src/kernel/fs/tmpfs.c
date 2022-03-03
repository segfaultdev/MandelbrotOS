#include <dev/device.h>
#include <drivers/rtc.h>
#include <fs/tmpfs.h>
#include <fs/vfs.h>
#include <mm/kheap.h>
#include <mm/vmm.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int tmpfs_readdir(fs_file_t *file, dirent_t *dirent, size_t pos) {
  if (pos == 0) {
    strcpy(dirent->name, ".");
    return 0;
  } else if (pos == 1) {
    strcpy(dirent->name, ".");
    return 0;
  }

  tmpfs_dirent_t *dir = (tmpfs_dirent_t *)file->private_data;

  for (size_t i = 0; i < (size_t)dir->children.length; i++)
    if (i == pos) {
      strcpy(dirent->name, dir->children.data[i]->path);
      return 0;
    }

  return 1;
}

int tmpfs_finddir(fs_file_t *file, char *name, dirent_t *dirent) {
  tmpfs_dirent_t *dir = (tmpfs_dirent_t *)file->private_data;
  for (size_t i = 0; i < (size_t)dir->children.length; i++)
    if (!strcmp(name, dir->children.data[i]->path)) {
      dirent->file_serial = dir->children.data[i]->inode;
      strcpy(dirent->name, dir->children.data[i]->path);
      return 0;
    }
  return 1;
}

#include <printf.h>

int tmpfs_find(fs_t *fs, char *path, fs_file_t **ref, fs_file_t **parent_dir) {
  fs_file_t *root = fs->private_data;

  if (*path == '/')
    path++;

  char *tok = kcalloc(strlen(path));

  while (*path) {
    for (size_t i = 0;; i++, path++) {
      if (*path == '/' || !*path) {
        tok[i] = 0;
        break;
      }
      tok[i] = *path;
    }

    if (parent_dir)
      *parent_dir = root;

    dirent_t dir;
    if ((tmpfs_finddir(root, tok, &dir))) {
      kfree(tok);
      return 1;
    }

    root = (fs_file_t *)dir.file_serial;
    memset(tok, 0, strlen(path));
  }

  kfree(tok);

  if (ref)
    *ref = root;

  return 0;
}

void *tmpfs_mmap(fs_file_t *file, pagemap_t *pg, syscall_file_t *sfile,
                 void *addr, size_t size, size_t offset, int prot, int flags) {
  (void)file;
  (void)pg;
  (void)addr;
  (void)size;
  (void)offset;
  (void)flags;
  (void)sfile;
  (void)prot;
  return NULL;
}

int tmpfs_close(fs_file_t *file) {
  (void)file;
  return 0;
}

int tmpfs_read(fs_file_t *file, uint8_t *buf, size_t offset, size_t count) {
  memcpy((void *)buf, file->private_data + offset, count);
  return 0;
}

int tmpfs_write(fs_file_t *file, uint8_t *buf, size_t offset, size_t count) {
  if (count + offset > file->length) {
    file->private_data = krealloc(file->private_data, count + offset);
    file->length = count + offset;
  }

  memcpy(file->private_data + offset, buf, count);

  return 0;
}

int tmpfs_rmdir(fs_file_t *file) {
  if (!S_ISDIR(file->mode))
    return 1;

  kfree(((tmpfs_dirent_t *)file->private_data)
            ->children
            .data); // TODO: This can leave hanging dirs. Make it *not* do that
  kfree(file->private_data);
  kfree(file);

  return 0;
}

int tmpfs_delete(fs_file_t *file) {
  if (S_ISDIR(file->mode))
    return 1;
  kfree(file->private_data);
  kfree(file);
  return tmpfs_close(file);
}

int tmpfs_truncate(fs_file_t *file, size_t size) {
  file->length = size;
  return 0;
}

uint64_t tmpfs_ioctl(fs_file_t *file, uint64_t cmd, void *arg) {
  (void)file;
  (void)cmd;
  (void)arg;
  return 0;
}

int tmpfs_chmod(fs_file_t *file, int mode) {
  file->mode = mode;
  return 0;
}

int tmpfs_chown(fs_file_t *file, int uid, int gid) {
  file->uid = uid;
  file->gid = gid;
  return 0;
}

fs_file_t *tmpfs_open(fs_t *fs, char *name) {
  fs_file_t *q_file = kmalloc(sizeof(fs_file_t));
  if ((tmpfs_find(fs, name, &q_file, NULL)))
    return NULL;

  return q_file;
}

file_ops_t tmpfs_file_ops = (file_ops_t){
    .read = tmpfs_read,
    .write = tmpfs_write,
    .rmdir = tmpfs_rmdir,
    .delete = tmpfs_delete,
    .truncate = tmpfs_truncate,
    .close = tmpfs_close,
    .readdir = tmpfs_readdir,
    .ioctl = tmpfs_ioctl,
    .chmod = tmpfs_chmod,
    .chown = tmpfs_chown,
};

fs_file_t *tmpfs_create(fs_t *fs, char *path, int mode, int uid, int gid) {
  posix_time_t tim = rtc_mktime(rtc_get_datetime());

  fs_file_t *parent_dir = kmalloc(sizeof(fs_file_t));

  if (!tmpfs_find(fs, path, NULL, &parent_dir))
    return tmpfs_open(fs, path);

  fs_file_t *file = kmalloc(sizeof(fs_file_t));
  *file = (fs_file_t){
      .uid = uid,
      .gid = gid,
      .file_ops = &tmpfs_file_ops,
      .fs = fs,
      .length = 0,
      .inode = (uint64_t)file,
      .private_data = kmalloc(1),
      .mode = S_IFREG | mode,

      .last_access_time = tim,
      .last_modification_time = tim,
      .last_status_change_time = tim,
      .creation_time = tim,
  };

  path++;

  char *short_name = kmalloc(strlen(path));

  for (size_t i = 0;; i++, path++) {
    if (*path == '/' || !*path) {
      short_name[i] = 0;
      break;
    }
    short_name[i] = *path;
  }

  file->path = strdup(short_name);

  kfree(short_name);

  vec_push(&((tmpfs_dirent_t *)parent_dir->private_data)->children, file);

  return file;
}

fs_file_t *tmpfs_mkdir(fs_t *fs, char *path, int mode, int uid, int gid) {
  posix_time_t tim = rtc_mktime(rtc_get_datetime());

  fs_file_t *parent_dir = kmalloc(sizeof(fs_file_t));

  if (!tmpfs_find(fs, path, NULL, &parent_dir))
    return tmpfs_open(fs, path);

  fs_file_t *file = kmalloc(sizeof(fs_file_t));
  *file = (fs_file_t){
      .uid = uid,
      .gid = gid,
      .file_ops = &tmpfs_file_ops,
      .fs = fs,
      .length = 0,
      .inode = (uint64_t)file,
      .private_data = kmalloc(sizeof(tmpfs_dirent_t)),
      .mode = S_IFDIR | mode,

      .last_access_time = tim,
      .last_modification_time = tim,
      .last_status_change_time = tim,
      .creation_time = tim,
  };

  path++;

  char *short_name = kmalloc(strlen(path));

  for (size_t i = 0;; i++, path++) {
    if (*path == '/' || !*path) {
      short_name[i] = 0;
      break;
    }
    short_name[i] = *path;
  }

  file->path = strdup(short_name);

  kfree(short_name);

  vec_push(&((tmpfs_dirent_t *)parent_dir->private_data)->children, file);

  ((tmpfs_dirent_t *)file->private_data)->parent = parent_dir;
  ((tmpfs_dirent_t *)file->private_data)->children.data =
      kmalloc(sizeof(fs_file_t));
  ((tmpfs_dirent_t *)file->private_data)->file = file;

  return file;
}

fs_file_t *tmpfs_mknod(fs_t *fs, char *name, int mode, int uid, int gid,
                       device_t *dev) {
  posix_time_t tim = rtc_mktime(rtc_get_datetime());

  fs_file_t *parent_dir = kmalloc(sizeof(fs_file_t));

  if (!tmpfs_find(fs, name, NULL, &parent_dir))
    return tmpfs_open(fs, name);

  fs_file_t *file = kmalloc(sizeof(fs_file_t));
  *file = (fs_file_t){
      .uid = uid,
      .gid = gid,
      .file_ops = &tmpfs_file_ops,
      .fs = fs,
      .length = 0,
      .inode = (uint64_t)file,
      .private_data = NULL,
      .mode = mode,
      .dev = dev,

      .last_access_time = tim,
      .last_modification_time = tim,
      .last_status_change_time = tim,
      .creation_time = tim,
  };

  name++;

  char *short_name = kmalloc(strlen(name));

  for (size_t i = 0;; i++, name++) {
    if (*name == '/' || !*name) {
      short_name[i] = 0;
      break;
    }
    short_name[i] = *name;
  }

  file->path = strdup(short_name);

  kfree(short_name);

  vec_push(&((tmpfs_dirent_t *)parent_dir->private_data)->children, file);

  return file;
}

fs_ops_t tmpfs_fs_ops;

fs_t *tmpfs_mount(device_t *dev) {
  fs_t *fs = kmalloc(sizeof(fs_t));
  fs->dev = dev;
  fs->ops = &tmpfs_fs_ops;

  posix_time_t tim = rtc_mktime(rtc_get_datetime());

  fs_file_t *file = kmalloc(sizeof(fs_file_t));
  *file = (fs_file_t){
      .uid = 0, // Idk know anymore
      .gid = 0,
      .file_ops = &tmpfs_file_ops,
      .fs = fs,
      .length = 0,
      .inode = (uint64_t)file,
      .private_data = kmalloc(sizeof(tmpfs_dirent_t)),
      .mode = S_IFDIR | 0777,

      .last_access_time = tim,
      .last_modification_time = tim,
      .last_status_change_time = tim,
      .creation_time = tim,
  };

  file->path = strdup("/");

  vec_push(&((tmpfs_dirent_t *)file->private_data)->children, file);

  ((tmpfs_dirent_t *)file->private_data)->parent = file;
  ((tmpfs_dirent_t *)file->private_data)->children.data =
      kmalloc(sizeof(fs_file_t));
  ((tmpfs_dirent_t *)file->private_data)->children.capacity = 1;
  ((tmpfs_dirent_t *)file->private_data)->children.length = 0;

  ((tmpfs_dirent_t *)file->private_data)->file = file;

  fs->private_data = file;

  return fs;
}

fs_ops_t tmpfs_fs_ops = (fs_ops_t){
    .create = tmpfs_create,
    .fs_name = "TMPFS",
    .mkdir = tmpfs_mkdir,
    .mount = tmpfs_mount,
    .open = tmpfs_open,
    .mknod = tmpfs_mknod,
    .post_mount = NULL, // Tf is a post mount :))))))
};

int init_tmpfs() {
  vfs_register_fs(&tmpfs_fs_ops);
  return 0;
}
