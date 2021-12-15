include config.mk

QEMU_DOCKER = $(QEMU) -curses

all: clean guard kernel prog $(HDD) qemu

kernel:
	@ cd src/kernel && make

prog:
	@ cd src/prog && make

qemu:
	@ $(QEMU)

qemu_docker: $(HDD)
	@ $(QEMU_DOCKER)

guard:
	@ $(DIRECTORY_GUARD)

$(HDD): $(KERNEL)
	@ echo "[HDD]"
	@ dd if=/dev/zero of=$@ bs=1M seek=64 count=0
	@ echo "drive c: file=\"$@\" partition=1" > resources/mtoolsrc
	@ MTOOLSRC=resources/mtoolsrc mpartition -I -s 64 -t 256 -h 8 c:
	@ MTOOLSRC=resources/mtoolsrc mpartition -c -b 128 -l 1048448 c:
	@ MTOOLSRC=resources/mtoolsrc mpartition -a c:
	@ MTOOLSRC=resources/mtoolsrc mformat -c 8 -F c:
	@ MTOOLSRC=resources/mtoolsrc mmd c:/boot
	@ MTOOLSRC=resources/mtoolsrc mmd c:/prog
	@ MTOOLSRC=resources/mtoolsrc mcopy resources/limine.cfg c:/boot
	@ MTOOLSRC=resources/mtoolsrc mcopy resources/limine.sys c:/boot
	@ MTOOLSRC=resources/mtoolsrc mcopy $< c:/boot/kernel
	@ MTOOLSRC=resources/mtoolsrc mcopy build/prog/* c:/prog
	@ ./resources/limine-install $@

toolchain:	
	# Init them
	@ echo "[CLONING SUBMODULES]"
	@ git submodule init
	@ git submodule update
	
	# Limine
	@ echo "[BUILD LIMINE]"
	@ cp -R thirdparty/limine/limine-install-linux-x86_64 resources/limine-install
	@ cp -R thirdparty/limine/limine.sys resources
	@ cp -R thirdparty/limine/BOOTX64.EFI resources
	 
	# Cross compiler
	@ echo "[BUILD CROSS COMPILER]"
	@ ./cross-compiler.sh

docker_build:
	@ docker build -f buildenv/Dockerfile . -t mandelbrot
	@ docker run -it mandelbrot make qemu_docker

clean:
	@ rm -rf $(BUILD_DIRECTORY) $(HDD) $(KERNEL)
