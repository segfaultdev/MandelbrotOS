#ifndef __TMPFS_H__
#define __TMPFS_H__

#include <fs/vfs.h>
#include <vec.h>

typedef struct tmpfs_dirent {
  fs_file_t *file;
  vec_t(fs_file_t *) children;
  fs_file_t *parent;
} tmpfs_dirent_t;

extern fs_ops_t tmpfs_fs_ops;
extern file_ops_t tmpfs_file_ops;

int init_tmpfs();

#endif
