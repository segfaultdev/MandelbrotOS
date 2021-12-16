include ../../../config.mk

LD = ../../../cross/bin/x86_64-elf-ld
AS = nasm

ASFLAGS := $(ASFLAGS) \
	-static \

ASFILES := $(shell find src/ -name '*.asm')
OFILES := $(ASFILES:.asm=.o)

TARGET = ../../../build/prog/asm

all: clean compile

compile: ld
	@ echo "Done!"
	
ld: $(OFILES)
	@ echo "[LD] $^"
	@ $(LD) -Tlinker.ld $^ -o $(TARGET)

%.o: %.asm
	@ echo "[AS] $<"
	@ $(AS) $(ASFLAGS) $< -o $@

clean:
	@ echo "[CLEAN]"
	@ rm -rf $(OFILES) $(TARGET)

