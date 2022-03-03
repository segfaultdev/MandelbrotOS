#include <dev/device.h>
#include <fs/vfs.h>
#include <mm/kheap.h>
#include <stdint.h>
#include <string.h>
#include <vec.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

vec_t(device_t *) devices;

int init_devices() {
  devices.data = kmalloc(sizeof(device_t));
  return 0;
}

int device_add(device_t *dev) {
  vec_push(&devices, dev);
  return devices.length - 1;
}

device_t *device_get_by_id(uint32_t id) {
  for (size_t i = 0; i < (size_t)devices.length; i++)
    if (devices.data[i]->id == id)
      return devices.data[i];
  return NULL;
}

uint8_t device_read(device_t *dev, size_t start, size_t count, uint8_t *buf) {
  if (S_ISBLK(dev->type)) {
    if (start % dev->block_size) {
      uint8_t *nbuf = kmalloc(dev->block_size);

      size_t len = MIN(dev->block_size - (start % dev->block_size), count);

      dev->read(dev, start / dev->block_size, 1, nbuf);

      memcpy(buf, nbuf + (start % dev->block_size), len);

      count -= len;
      buf += len;
      start += len;

      kfree(nbuf);

      if (!count)
        return 0;
    }

    size_t blocks = count / dev->block_size;

    if (blocks) {
      dev->read(dev, start / dev->block_size, blocks, buf);

      count -= blocks * dev->block_size;
      buf += blocks * dev->block_size;
      start += blocks * dev->block_size;

      if (!count)
        return 0;
    }

    size_t end = count % dev->block_size;

    if (end) {
      uint8_t *nbuf = kmalloc(dev->block_size);
      dev->read(dev, start / dev->block_size, 1, nbuf);
      memcpy(buf, nbuf + (start % dev->block_size), end);
      kfree(nbuf);
    }

    return 0;
  } else if (S_ISCHR(dev->type))
    return dev->read(dev, start, count, buf);

  return 1;
}

uint8_t device_write(device_t *dev, size_t start, size_t count, uint8_t *buf) {
  if (S_ISBLK(dev->type)) {
    if (start % dev->block_size) {
      uint8_t *nbuf = kmalloc(dev->block_size);

      size_t len = MIN(dev->block_size - (start % dev->block_size), count);

      dev->read(dev, start / dev->block_size, 1, nbuf);
      memcpy(nbuf + (start % dev->block_size), buf, len);
      dev->write(dev, start / dev->block_size, 1, nbuf);

      count -= len;
      buf += len;
      start += len;

      kfree(nbuf);

      if (!count)
        return 0;
    }

    size_t blocks = count / dev->block_size;

    if (blocks) {
      dev->write(dev, start / dev->block_size, blocks, buf);

      count -= blocks * dev->block_size;
      buf += blocks * dev->block_size;
      start += blocks * dev->block_size;

      if (!count)
        return 0;
    }

    size_t end = count % dev->block_size;

    if (end) {
      uint8_t *nbuf = kmalloc(dev->block_size);
      dev->read(dev, start / dev->block_size, 1, nbuf);
      memcpy(nbuf, buf, end);
      dev->write(dev, start / dev->block_size, 1, nbuf);
      kfree(nbuf);
    }

    return 0;
  } else if (S_ISCHR(dev->type))
    return dev->write(dev, start, count, buf);

  return 1;
}

device_t *device_get(uint32_t id) { return devices.data[id]; }
