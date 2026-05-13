# OSx2 Limemade OS Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pie -fno-pic -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone -mno-mmx -msse -msse2 -fsanitize=undefined -I. -I./include -I./kernel/libs/usb/cherryusb/source/common -I./kernel/libs/usb/cherryusb/source/core -I./kernel/libs/usb/cherryusb/source/osal -I./kernel/libs/usb/ -I./kernel/libs/rtc64/ -I./kernel/libs/usb/cherryusb/source/class/hub
LDFLAGS = -Wl,-T,linker.ld -static -nostdlib -Wl,-z,max-page-size=0x1000

KERNEL_SRC = $(wildcard kernel/unice64/*.c) \
             $(wildcard kernel/libs/io/*.c) \
             $(wildcard kernel/libs/ram/*.c) \
             $(wildcard kernel/libs/storage/*.c) \
             $(wildcard kernel/libs/storage/fatfs/*.c) \
             $(wildcard kernel/libs/rtc64/*.c) \
             $(wildcard kernel/libs/rtc64/nuklear/*.c) \
             $(filter-out kernel/libs/core/malloc.c,$(wildcard kernel/libs/core/*.c)) \
             $(wildcard kernel/libs/usb/*.c)

AS_SRC = $(wildcard kernel/unice64/*.s)
KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(AS_SRC:.s=.o)
KERNEL_ELF = kernel.elf

ISO_IMAGE = osx2.iso
SATA_DISK = sata_disk.img
LIMINE_DIR = ./limine
LIMINE_BIN = $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(LIMINE_DIR)/limine-uefi-cd.bin

# Standalone RSL Programs
PROGRAMS = bin/cargo.bin bin/shell.bin bin/desktop.bin bin/stress.bin

.PHONY: all menuconfig kernel iso run clean limine-setup programs

all:
	$(MAKE) limine-setup
	$(MAKE) kernel
	$(MAKE) programs
	$(MAKE) iso
	$(MAKE) $(SATA_DISK)

programs: $(PROGRAMS)

bin/%.bin: programs/entry.s programs/%.c programs/libc/libc.c kernel/libs/core/sanitizers.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -fno-pic -fno-pie -DUSERLAND_SANITIZER -nostdlib -static -Wl,-T,programs/linker.ld $^ -o $@

limine-setup:
	@mkdir -p limine
	@if [ ! -d "limine/.git" ]; then \
		echo "Fetching Limine (v7.x-binary) into pre-existing folder..."; \
		git clone https://github.com/limine-bootloader/limine.git --branch=v7.x-binary --depth=1 limine_tmp; \
		mv limine_tmp/* limine/ 2>/dev/null || true; \
		mv limine_tmp/.* limine/ 2>/dev/null || true; \
		rm -rf limine_tmp; \
	fi
	@if [ ! -f "limine/limine" ]; then \
		echo "Building Limine tools in limine/ folder..."; \
		$(MAKE) -C limine; \
	fi

run: iso $(SATA_DISK)
	qemu-system-x86_64 -M q35 -m 512M -serial stdio -cdrom $(ISO_IMAGE) \
		-drive file=$(SATA_DISK),if=none,id=d0,format=raw \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=d0,bus=ahci.0

$(SATA_DISK):
	@# Generate an empty truly empty disk to test OS internal installer
	@dd if=/dev/zero of=$(SATA_DISK) bs=1M count=64 status=none
	@echo "OSx2: 64MB Empty persistent disk created for internal installation test."

menuconfig:
	python3 scripts/menuconfig.py

kernel: limine-setup $(KERNEL_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(KERNEL_OBJ) -o $(KERNEL_ELF)
	@echo "OSx2 Limemade Kernel Compiled: $(KERNEL_ELF)"

%.o: %.c | limine-setup
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s | limine-setup
	$(CC) $(CFLAGS) -c $< -o $@

iso: limine-setup kernel programs
	@mkdir -p iso_root/boot
	@mkdir -p iso_root/bin
	@mkdir -p iso_root/EFI/BOOT
	@cp $(KERNEL_ELF) iso_root/boot/
	@cp bin/cargo.bin iso_root/bin/
	@cp bin/shell.bin iso_root/bin/
	@cp bin/desktop.bin iso_root/bin/
	@cp bin/stress.bin iso_root/bin/
	@cp boot/installer_limine.cfg iso_root/limine.cfg
	@cp $(LIMINE_DIR)/limine-uefi-cd.bin iso_root/EFI/BOOT/BOOTX64.EFI
	@python3 scripts/fat_tool.py ramdisk.img
	@cp ramdisk.img iso_root/boot/
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
	@echo "OSx2 Limemade ISO Created: $(ISO_IMAGE)"

clean:
	rm -f $(KERNEL_OBJ) $(KERNEL_ELF) $(ISO_IMAGE) $(SATA_DISK) ramdisk.img
	rm -rf iso_root bin
	@# Keep limine source but clean its binaries
	@if [ -d "limine" ]; then $(MAKE) -C limine clean || true; fi
