#include <drivers/ahci.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <klog.h>
#include <vec.h>

vec_t(fs_mountpoint_t) vfs_mounts = {0};

int init_vfs() {
  int root_disk_found = 0;
  uint8_t drive_name = 'A';

  for (size_t i = 0; i < (size_t)fat_parts.length; i++) {
    fs_mountpoint_t node = fat_partition_to_fs_node(i);
    strcpy(node.type, "FAT32");
    node.drive_num = drive_name++;
    node.position_in_list = i;

    vec_push(&vfs_mounts, node);

    if (!root_disk_found && node.is_root_partition) {
      root_disk_found = 1;
      vec_swap(&vfs_mounts, 0, i);
    }
  }

  if (!vfs_mounts.length)
    return 1;

  return 0;
}

uint8_t vfs_read(char *path, size_t offset, size_t count, uint8_t *buffer) {
  if (!buffer)
    return 1;
  if (strlen(path) < 2)
    return 1;
  if (path[1] != ':')
    return 1;
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return 1;
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return 1;
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.read)
    return root_node.read(root_node.position_in_list, path + 2, offset, buffer,
                          count);

  return 1;
}

uint8_t vfs_write(char *path, size_t offset, size_t count, uint8_t *buffer) {
  if (!buffer)
    return 1;
  if (strlen(path) < 2)
    return 1;
  if (path[1] != ':')
    return 1;
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return 1;
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return 1;
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.write)
    return root_node.write(root_node.position_in_list, path + 2, offset, buffer,
                           count);

  return 1;
}

size_t vfs_sizeof_file(char *path) {
  if (strlen(path) < 2)
    return -1;
  if (path[1] != ':')
    return -1;
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return -1;
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return -1;
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.sizeof_file)
    return root_node.sizeof_file(root_node.position_in_list, path + 2);

  return -1;
}

uint8_t vfs_identify(char *path) {
  if (strlen(path) < 2)
    return 1;
  if (path[1] != ':')
    return 1;
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return 1;
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return 1;
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.identify)
    return root_node.identify(root_node.position_in_list, path + 2);

  return 1;
}

uint8_t vfs_delete(char *path) {
  if (strlen(path) < 2)
    return 1;
  if (path[1] != ':')
    return 1;
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return 1;
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return 1;
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.delete)
    return root_node.delete(root_node.position_in_list, path + 2);

  return 1;
}

uint8_t vfs_create_file(char *path) {
  if (strlen(path) < 2)
    return 1;
  if (path[1] != ':')
    return 1;
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return 1;
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return 1;
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.create_file)
    return root_node.create_file(root_node.position_in_list, path + 2);

  return 1;
}

uint8_t vfs_create_dir(char *path) {
  if (strlen(path) < 2)
    return 1;
  if (path[1] != ':')
    return 1;
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return 1;
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return 1;
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.mkdir)
    return root_node.mkdir(root_node.position_in_list, path + 2);

  return 1;
}

vec_fs_file_t vfs_list_dir(char *path) {
  if (strlen(path) < 2)
    return (vec_fs_file_t){0};
  if (path[1] != ':')
    return (vec_fs_file_t){0};
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return (vec_fs_file_t){0};
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return (vec_fs_file_t){0};
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.mkdir)
    return root_node.list_directory(root_node.position_in_list, path + 2);

  return (vec_fs_file_t){0};
}

fs_file_t vfs_get_info(char *path) {
  if (strlen(path) < 2)
    return (fs_file_t){0};
  if (path[1] != ':')
    return (fs_file_t){0};
  if (!(path[0] >= 'A' && path[0] <= 'Z')) {
    path[0] = path[0] - 'a' + 'A';

    if (!(path[0] >= 'A' && path[0] <= 'Z'))
      return (fs_file_t){0};
    if (path[0] - 'A' + 1 > vfs_mounts.length)
      return (fs_file_t){0};
  }

  fs_mountpoint_t root_node = vfs_mounts.data[path[0] - 'A'];
  if (root_node.info)
    return root_node.info(root_node.position_in_list, path + 2);

  return (fs_file_t){0};
}
