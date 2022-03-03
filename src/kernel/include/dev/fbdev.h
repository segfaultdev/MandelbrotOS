#ifndef __FBDEV_H__
#define __FBDEV_H__

#define IOCTL_FBDEV_GET_WIDTH 1
#define IOCTL_FBDEV_GET_HEIGHT 2
#define IOCTL_FBDEV_GET_BPP 3

int init_fbdev(char *mount_place);

#endif
