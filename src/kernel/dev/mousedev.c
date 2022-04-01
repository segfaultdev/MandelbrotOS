#include <dev/device.h>
#include <dev/mousedev.h>
#include <drivers/ps2.h>
#include <fs/vfs.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>

void *mousedev_mmap(device_t *dev, pagemap_t *pg, syscall_file_t *sfile,
                    void *addr, size_t size, size_t offset, int prot,
                    int flags) {
  (void)dev;
  (void)pg;
  (void)sfile;
  (void)addr;
  (void)size;
  (void)offset;
  (void)prot;
  (void)flags;
  return NULL;
}

ssize_t mousedev_write(device_t *dev, size_t start, size_t count,
                       uint8_t *buf) {
  (void)start;
  (void)dev;
  (void)count;
  (void)buf;
  return 0;
}

ssize_t mousedev_read(device_t *dev, size_t start, size_t count, uint8_t *buf) {
  (void)dev;
  (void)start;
  (void)count;
  (void)buf;

  for (size_t i = 0; i < count; i += 3)
    getmouse((int8_t *)buf + i);

  return count;
}

uint64_t mousedev_ioctl(device_t *dev, uint64_t cmd, void *arg) {
  (void)dev;
  (void)arg;
  (void)cmd;

  return -1;
}

device_t mouse0 = (device_t){
    .block_count = 0,
    .block_size = 3,
    .fs = NULL,
    .id = 1011,
    .name = "mouse0",
    .private_data = NULL,
    .read = mousedev_read,
    .write = mousedev_write,
    .mmap = mousedev_mmap,
    .ioctl = mousedev_ioctl,
    .type = S_IFBLK,
};

int init_mousedev(char *mount_place) {
  char *str = kcalloc(strlen(mount_place) + 10);
  strcpy(str, mount_place);

  if (str[strlen(str) - 1] != '/')
    str[strlen(str)] = '/';

  strcat(str, "mouse0");

  device_add(&mouse0);

  int val = (vfs_mknod(str, 0777 | S_IFCHR, 0, 0, &mouse0)) ? 0 : 1;
  kfree(str);

  return val;
}
