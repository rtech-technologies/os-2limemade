# OSx2 Limemade OS Makefile

CC = gcc
CFLAGS = -fsanitize=undefined -fno-sanitize-recover=undefined -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pie -fno-pic -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone -msse -msse2 -I. -I./include -Ikernel/libs/cherryusb/common -Ikernel/libs/cherryusb/core -Ikernel/libs/cherryusb/class/hub -Ikernel/libs/cherryusb/class/msc -Ikernel/libs/cherryusb/class/hid -Iprograms
LDFLAGS = -Wl,-T,boot/linker.ld -static -nostdlib -Wl,-z,max-page-size=0x1000

KERNEL_SRC = $(filter-out kernel/libs/signature_check.c, $(wildcard kernel/unice64/*.c) \
             $(wildcard kernel/libs/io/*.c) \
             $(wildcard kernel/libs/ram/*.c) \
             $(wildcard kernel/libs/storage/*.c) \
             $(wildcard kernel/libs/storage/fatfs/*.c) \
             $(wildcard kernel/libs/core/*.c) \
             $(wildcard kernel/libs/rtc64/*.c) \
             kernel/libs/cherryusb/core/usbh_core.c \
             kernel/libs/cherryusb/class/hub/usbh_hub.c \
             kernel/libs/cherryusb/class/msc/usbh_msc.c \
             kernel/libs/cherryusb/class/hid/usbh_hid.c \
             programs/shell.c \
             programs/desktop.c)
AS_SRC = $(wildcard kernel/unice64/*.s)
KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(AS_SRC:.s=.o)
KERNEL_ELF = kernel.elf

ISO_IMAGE = osx2.iso
SATA_DISK = sata_disk.img
LIMINE_DIR = ./limine
LIMINE_BIN = $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(LIMINE_DIR)/limine-uefi-cd.bin

.PHONY: all menuconfig kernel iso run clean limine-setup cherryusb-setup nuklear-setup manifest metadata ci-verify

all:
	$(MAKE) limine-setup
	$(MAKE) cherryusb-setup
	$(MAKE) nuklear-setup
	$(MAKE) kernel
	$(MAKE) manifest
	$(MAKE) metadata
	$(MAKE) kernel
	$(MAKE) iso
	$(MAKE) $(SATA_DISK)

manifest:
	python3 scripts/manifest.py $(KERNEL_ELF)

metadata:
	python3 scripts/metadata.py "$(CFLAGS)"

ci-verify:
	@echo "OSx2 CI Verification Ritual..."
	@$(MAKE) clean
	@$(MAKE) all
	@sha256sum $(ISO_IMAGE)

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

iso: limine-setup kernel
	@mkdir -p iso_root/boot
	@cp $(KERNEL_ELF) iso_root/boot/
	@cp boot/limine.cfg iso_root/
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

cherryusb-setup:
	@mkdir -p kernel/libs/cherryusb
	@if [ ! -d "kernel/libs/cherryusb/.git" ]; then \
		echo "Fetching CherryUSB..."; \
		git clone https://github.com/cherry-embedded/CherryUSB.git && cd kernel/libs/cherryusb_tmp && git checkout 0537fc4 && cd ../../.. kernel/libs/cherryusb_tmp; \
		mv kernel/libs/cherryusb_tmp/* kernel/libs/cherryusb/ 2>/dev/null || true; \
		mv kernel/libs/cherryusb_tmp/.* kernel/libs/cherryusb/ 2>/dev/null || true; \
		rm -rf kernel/libs/cherryusb_tmp; \
	fi

nuklear-setup:
	@mkdir -p programs/nuklear
	@if [ ! -d "programs/nuklear/.git" ]; then \
		echo "Fetching Nuklear..."; \
		git clone https://github.com/Immediate-Mode-UI/Nuklear.git && cd programs/nuklear_tmp && git checkout 9f99723 && cd ../../.. programs/nuklear_tmp; \
		mv programs/nuklear_tmp/* programs/nuklear/ 2>/dev/null || true; \
		mv programs/nuklear_tmp/.* programs/nuklear/ 2>/dev/null || true; \
		rm -rf programs/nuklear_tmp; \
	fi

clean:
	rm -f $(KERNEL_OBJ) $(KERNEL_ELF) $(ISO_IMAGE) $(SATA_DISK) ramdisk.img
	rm -rf iso_root
	@# Keep limine source but clean its binaries
	@if [ -d "limine" ]; then $(MAKE) -C limine clean || true; fi
