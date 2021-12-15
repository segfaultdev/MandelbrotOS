include ../../../config.mk

LD = ld
AS = nasm

AFLAGS := \
	-felf64 -O3 \
	-static \

AFILES := $(shell find src/ -name '*.asm')
OFILES := $(AFILES:.asm=.o)

TARGET = ../../../build/prog/asm

all: clean compile

compile: ld
	@ echo "Done!"
	
ld: $(OFILES)
	@ echo "[LD] $^"
	@ $(LD) -Tlinker.ld $^ -o $(TARGET)

%.o: %.asm
	@ echo "[AS] $<"
	@ $(AS) $(AFLAGS) $< -o $@

clean:
	@ echo "[CLEAN]"
	@ rm -rf $(OFILES) $(TARGET)

