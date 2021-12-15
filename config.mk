ARCH = x86_64

LD = ld
CC = gcc
AS = nasm

HDD = mandelbrotos.hdd

# QEMU = qemu-system-$(ARCH) -hdd $(HDD) -smp 1 -M q35 -soundhw pcspk -serial stdio -rtc base=localtime
QEMU = qemu-system-$(ARCH) -hdd $(HDD) -smp 2 -M q35 -soundhw pcspk -serial stdio -rtc base=localtime
# QEMU = qemu-system-$(ARCH) -hdd $(HDD) -smp 1 -M q35 -soundhw pcspk -monitor stdio -d int -rtc base=localtime

KERNEL = build/kernel/mandelbrotos.elf

BUILD_DIRECTORY = build
KERNEL_BUILD_DIRECTORY := ../../build/kernel/
PROG_BUILD_DIRECTORY := ../../build/prog/
DIRECTORY_GUARD = mkdir -p build/

ASFLAGS = -f elf64 -O3

CFLAGS := \
	-Werror \
	-Wall \
	-Wextra \
	-std=gnu99 \
	-O3 \
	-pipe \