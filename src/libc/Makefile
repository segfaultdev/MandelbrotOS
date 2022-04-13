include ../../config.mk

AR = ../../cross/bin/x86_64-elf-ar
CC = ../../cross/bin/x86_64-elf-gcc
AS = nasm

CFLAGS := $(CFLAGS) \
	-Isrc/include \
	-ffreestanding \
	-mno-red-zone \
	-fno-pic -no-pie \

ASFLAGS := $(ASFLAGS) \
	\

CFILES := $(shell find src/ -name '*.c')
ASFILES := $(shell find src/ -name '*.asm')
OFILES := $(CFILES:.c=.o) $(ASFILES:.asm=.o)

OFILES := $(patsubst src/%.c, ../../build/libc/%.c.o, $(CFILES)) $(patsubst src/%.asm, ../../build/libc/%.asm.o, $(ASFILES))

TARGET = ../../build/libc/libc.a

all: clean compile

compile: ar
	@ echo "Done!"
	
ar: $(OFILES)
	@ echo "[AR] $^"
	$(AR) rcu $(TARGET) $^

../../build/libc/%.c.o: src/%.c
	@ echo "[CC] $<"
	@ mkdir -p $(shell dirname $@)
	@ $(CC) $(CFLAGS) -c $< -o $@

../../build/libc/%.asm.o: src/%.asm
	@ echo "[AS] $<"
	@ mkdir -p $(shell dirname $@)
	@ $(AS) $(ASFLAGS) $< -o $@

clean:
	@ echo "[CLEAN]"
	@ rm -rf $(OFILES) $(TARGET)
