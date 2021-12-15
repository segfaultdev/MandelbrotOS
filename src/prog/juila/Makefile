include ../../../config.mk

LD = ld
CC = gcc

CFLAGS := -O3 \
	-Isrc/include \
	-ffreestanding \
	-mno-red-zone \
	-fno-pic -no-pie \
	-static \

AFLAGS := \
	-felf64 -O3 \
	-static \

LDFLAGS := \
	-Tlinker.ld \

CFILES := $(shell find src/ -name '*.c')
AFILES := $(shell find src/ -name '*.asm')
OFILES := $(CFILES:.c=.o) $(AFILES:.asm=.o)

TARGET = ../../../build/prog/julia

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
	@ $(AS) $(AFLAGS) $< -o $@

clean:
	@ echo "[CLEAN]"
	@ rm -rf $(OFILES) $(TARGET)
