# Sovereign OS Makefile

CC = gcc
LD = ld
CFLAGS = -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pie -fno-pic -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I. -I./include
LDFLAGS = -T boot/linker.ld -static -nostdlib -z max-page-size=0x1000

KERNEL_SRC = $(wildcard kernel/unice64/*.c) $(wildcard kernel/libs/*.c) programs/shell.c
KERNEL_OBJ = $(KERNEL_SRC:.c=.o)
KERNEL_ELF = kernel.elf

ISO_IMAGE = osx2.iso

.PHONY: all menuconfig kernel iso run clean

all: kernel iso

run: iso
	qemu-system-x86_64 -M q35 -m 512M -serial stdio -cdrom $(ISO_IMAGE)

menuconfig:
	python3 scripts/menuconfig.py

kernel: $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) $(KERNEL_OBJ) -o $(KERNEL_ELF)
	@echo "Sovereign Kernel Compiled: $(KERNEL_ELF)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

iso: kernel
	@mkdir -p iso_root
	@cp $(KERNEL_ELF) iso_root/
	@# In a real scenario, copy limine.cfg and limine bootloader files here
	@python3 scripts/fat_tool.py ramdisk.img
	@cp ramdisk.img iso_root/
	@touch $(ISO_IMAGE)
	@echo "Sovereign ISO Created: $(ISO_IMAGE)"

clean:
	rm -f $(KERNEL_OBJ) $(KERNEL_ELF) $(ISO_IMAGE) ramdisk.img
	rm -rf iso_root
