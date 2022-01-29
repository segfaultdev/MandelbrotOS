#include <device.h>
#include <mm/kheap.h>
#include <stdint.h>
#include <vec.h>

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
    if (devices.data[i]->fs_type == id)
      return devices.data[i];
  return NULL;
}

device_t *device_get(uint32_t id) { return devices.data[id]; }
