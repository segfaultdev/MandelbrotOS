#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stddef.h>
#include <stdint.h>

#define DEVICE_UNKOWN 0
#define DEVICE_BLOCK 1
#define DEVICE_CHAR 2

#define DEVICE_FS_DISK 1
#define DEVICE_FS_DEV 2

struct fs;

typedef struct device {
  char *name;
  uint8_t type;
  uint32_t fs_type;
  struct fs *fs;
  void *private_data;
  void *private_data2;
  uint8_t (*read)(struct device *dev, uint64_t start, uint32_t count,
                  uint8_t *buf);
  uint8_t (*write)(struct device *dev, uint64_t start, uint32_t count,
                   uint8_t *buf);
} device_t;

int device_add(device_t *dev);
device_t *device_get(uint32_t id);

#endif
