#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <mm/vmm.h>
#include <stddef.h>
#include <stdint.h>

typedef signed long long int ssize_t;
struct fs;

typedef struct device {
  char *name;
  uint64_t type;
  uint64_t id;
  struct fs *fs;
  void *private_data;
  ssize_t (*read)(struct device *dev, size_t start, size_t count, uint8_t *buf);
  ssize_t (*write)(struct device *dev, size_t start, size_t count,
                   uint8_t *buf);
  uint64_t (*ioctl)(struct device *dev, uint64_t cmd, void *arg);
  void *(*mmap)(struct device *dev, pagemap_t *pg, syscall_file_t *sfile,
                void *addr, size_t size, size_t offset, int prot, int flags);

  size_t block_size;
  size_t block_count;
} device_t;

ssize_t device_read(device_t *dev, size_t start, size_t count, uint8_t *buf);
ssize_t device_write(device_t *dev, size_t start, size_t count, uint8_t *buf);
int device_add(device_t *dev);
device_t *device_get(uint32_t id);

#endif
