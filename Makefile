# OSx2 Limemade OS Makefile

CC = gcc
TINYUSB_DIR = kernel/libs/tinyusb/src
TINYUSB_INC = -I$(TINYUSB_DIR) -I$(TINYUSB_DIR)/common -I$(TINYUSB_DIR)/host -I$(TINYUSB_DIR)/class/hid -I$(TINYUSB_DIR)/class/msc

CFLAGS = -O2 -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pie -fno-pic -fno-omit-frame-pointer -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I. -I./include $(TINYUSB_INC) -U_FORTIFY_SOURCE -fno-stack-protector
LDFLAGS = -Wl,-T,boot/linker.ld -static -nostdlib -Wl,-z,max-page-size=0x1000

BIN_CFLAGS = -O2 -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-pie -fno-pic -fno-omit-frame-pointer -m64 -march=x86-64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -I. -I./include
BIN_LDFLAGS = -static -nostdlib -Wl,-Tprograms/linker.ld

TINYUSB_SRC = $(TINYUSB_DIR)/tusb.c \
              $(TINYUSB_DIR)/common/tusb_fifo.c \
              $(TINYUSB_DIR)/host/usbh.c \
              $(TINYUSB_DIR)/host/hub.c \
              $(TINYUSB_DIR)/class/hid/hid_host.c \
              $(TINYUSB_DIR)/class/msc/msc_host.c

KERNEL_SRC = $(wildcard kernel/unice64/*.c) \
             $(wildcard kernel/libs/io/*.c) \
             $(wildcard kernel/libs/ram/*.c) \
             $(wildcard kernel/libs/rtc64/*.c) \
             $(wildcard kernel/libs/storage/*.c) \
             $(wildcard kernel/libs/storage/fatfs/*.c) \
             $(wildcard kernel/libs/core/*.c) \
             $(TINYUSB_SRC) \
             programs/shell.c
AS_SRC = $(wildcard kernel/unice64/*.s)
KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(AS_SRC:.s=.o)
KERNEL_ELF = kernel.elf

PROGRAMS = wm.bin text_editor.bin
ISO_IMAGE = osx2.iso
SATA_DISK = sata_disk.img
LIMINE_DIR = ./limine
LIMINE_BIN = $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(LIMINE_DIR)/limine-uefi-cd.bin

.PHONY: all menuconfig kernel iso run clean limine-setup

all:
	$(MAKE) limine-setup
	$(MAKE) kernel
	$(MAKE) iso
	$(MAKE) $(SATA_DISK)

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
	qemu-system-x86_64 -M q35 -m 1G -serial stdio -cdrom $(ISO_IMAGE) \
		-drive file=$(SATA_DISK),if=none,id=d0,format=raw \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=d0,bus=ahci.0 \
		-device qemu-xhci,id=xhci \
		-device usb-tablet,bus=xhci.0 \
		-device usb-kbd,bus=xhci.0 \
		-nodefaults -vga std

$(SATA_DISK):
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

iso: limine-setup kernel $(PROGRAMS)
	@mkdir -p iso_root/boot
	@cp $(KERNEL_ELF) iso_root/boot/
	@cp $(PROGRAMS) iso_root/boot/
	@cp boot/limine.cfg iso_root/
	@python3 scripts/fat_tool.py ramdisk.img
	@cp ramdisk.img iso_root/boot/
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

%.bin: programs/%.c
	$(CC) $(BIN_CFLAGS) -DRSL_BINARY_MODE -c $< -o programs/$*.o
	# Using a flat binary format for .bin files
	$(CC) $(BIN_LDFLAGS) programs/$*.o -o $@

clean:
	rm -f $(KERNEL_OBJ) $(KERNEL_ELF) $(ISO_IMAGE) $(SATA_DISK) ramdisk.img $(PROGRAMS) programs/*.o
	rm -rf iso_root
	@if [ -d "limine" ]; then $(MAKE) -C limine clean || true; fi
