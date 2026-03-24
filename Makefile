# Sovereign OS Makefile

CC = gcc
LD = ld
CFLAGS = -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pie -fno-pic -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I. -I./include
LDFLAGS = -T boot/linker.ld -static -nostdlib -z max-page-size=0x1000

KERNEL_SRC = $(wildcard kernel/unice64/*.c) $(wildcard kernel/libs/*.c) $(wildcard kernel/libs/fatfs/*.c) programs/shell.c
KERNEL_OBJ = $(KERNEL_SRC:.c=.o)
KERNEL_ELF = kernel.elf

ISO_IMAGE = osx2.iso
LIMINE_DIR = ./limine
LIMINE_BIN = $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(LIMINE_DIR)/limine-uefi-cd.bin

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
	@cp boot/limine.cfg iso_root/
	@python3 scripts/fat_tool.py ramdisk.img
	@cp ramdisk.img iso_root/
	@# The Xorriso Ritual for Hybrid Boot (BIOS + UEFI)
	@if command -v xorriso >/dev/null 2>&1; then \
		cp $(LIMINE_BIN) iso_root/; \
		xorriso -as mkisofs -b limine-bios-cd.bin \
			-no-emul-boot -boot-load-size 4 -boot-info-table \
			--efi-boot limine-uefi-cd.bin \
			-efi-boot-part --efi-boot-image --protective-msdos-label \
			iso_root -o $(ISO_IMAGE); \
		$(LIMINE_DIR)/limine bios-install $(ISO_IMAGE); \
	else \
		touch $(ISO_IMAGE); \
		echo "Warning: xorriso not found, created empty $(ISO_IMAGE) for source compliance."; \
	fi
	@echo "Sovereign ISO Created: $(ISO_IMAGE)"

clean:
	rm -f $(KERNEL_OBJ) $(KERNEL_ELF) $(ISO_IMAGE) ramdisk.img
	rm -rf iso_root
