include ../../../config.mk

LD = ../../../cross/bin/x86_64-elf-ld
CC = ../../../cross/bin/x86_64-elf-gcc
AS = nasm

CFLAGS := $(CFLAGS) \
	-Isrc/include \
	-ffreestanding \
	-mno-red-zone \
	-fno-pic -no-pie \
	-static \

ASFLAGS := $(ASFLAGS) \
	-static \

LDFLAGS := \
	-Tlinker.ld \

CFILES := $(shell find src/ -name '*.c')
ASFILES := $(shell find src/ -name '*.asm')
OFILES := $(CFILES:.c=.o) $(ASFILES:.asm=.o)

TARGET = ../../../build/prog/test

all: clean compile

compile: ld
	@ echo "Done!"
	
ld: $(OFILES)
	@ echo "[LD] $^"
	@ $(LD) $(LDFLAGS) $^ -o $(TARGET)

%.o: %.c
	@ echo "[CC] $<"
	@ $(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	@ echo "[AS] $<"
	@ $(AS) $(ASFLAGS) $< -o $@

clean:
	@ echo "[CLEAN]"
	@ rm -rf $(OFILES) $(TARGET)
