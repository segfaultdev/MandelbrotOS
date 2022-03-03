#include <dev/device.h>
#include <dev/fbdev.h>
#include <fb/fb.h>
#include <fs/vfs.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <printf.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>

void *fb_mmap(device_t *dev, pagemap_t *pg, syscall_file_t *sfile, void *addr,
              size_t size, size_t offset, int prot, int flags) {
  (void)dev;

  for (size_t i = 0; i < size; i += PAGE_SIZE)
    vmm_map_page(
        pg,
        (uintptr_t)vmm_virt_to_phys(&kernel_pagemap, (uintptr_t)framebuffer) +
            i,
        (uintptr_t)addr + i, (prot & PROT_WRITE) ? 0b111 : 0b101);

  mmap_range_t *mmap_range = kmalloc(sizeof(mmap_range_t));
  *mmap_range = (mmap_range_t){
      .file = sfile,
      .flags = flags,
      .length = size,
      .offset = offset,
      .prot = prot,
      .phys_addr = vmm_virt_to_phys(&kernel_pagemap, (uintptr_t)framebuffer),
      .virt_addr = (uintptr_t)addr,
  };

  printf("FB: prot %lu\r\n", prot & PROT_WRITE);

  vec_push(&pg->ranges, mmap_range);

  return (void *)vmm_virt_to_phys(&kernel_pagemap, (uintptr_t)framebuffer);
}

uint8_t fb_write(device_t *dev, size_t start, size_t count, uint8_t *buf) {
  (void)start;
  (void)dev;
  (void)count;
  (void)buf;
  return 1;
}

uint8_t fb_read(device_t *dev, size_t start, size_t count, uint8_t *buf) {
  (void)dev;
  (void)start;
  (void)count;
  (void)buf;
  return 1; // TODO: Dya think it might be cool to maybe not just return 1 and
            // like, read from the keyboard
}

uint64_t fb_ioctl(device_t *dev, uint64_t cmd, void *arg) {
  (void)dev;
  (void)arg;

  switch (cmd) {
    case IOCTL_FBDEV_GET_HEIGHT:
      return fb_height;
      break;
    case IOCTL_FBDEV_GET_WIDTH:
      return fb_width;
      break;
    case IOCTL_FBDEV_GET_BPP:
      return 32;
      break;
  }

  return -1;
}

device_t fb0 = (device_t){
    .block_count = 0,
    .block_size = 0,
    .fs = NULL,
    .id = 1010,
    .name = "fb0",
    .private_data = NULL,
    .read = fb_read,
    .write = fb_write,
    .mmap = fb_mmap,
    .ioctl = fb_ioctl,
    .type = S_IFCHR,
};

int init_fbdev(char *mount_place) {
  char *str = kcalloc(strlen(mount_place) + 7);
  strcpy(str, mount_place);

  if (str[strlen(str) - 1] != '/')
    str[strlen(str)] = '/';

  strcat(str, "fb0");

  device_add(&fb0);

  int val = (vfs_mknod(str, 0777 | S_IFCHR, 0, 0, &fb0)) ? 0 : 1;
  kfree(str);
  return val;
}
