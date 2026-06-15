# OSx2 Limemade OS Makefile

CC = gcc
# SSE Support Enabled: Standard for x86_64, but explicit here for Sovereign ISA compliance
CFLAGS = -Wall -Wextra -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check \
         -fno-lto -fno-pie -fno-pic -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone \
         -msse -msse2 -I. -I./include
LDFLAGS = -Wl,-T,boot/linker.ld -static -nostdlib -Wl,-z,max-page-size=0x1000

# Kernel core objects
KERNEL_SRC = $(wildcard kernel/unice64/*.c) \
             $(wildcard kernel/libs/io/*.c) \
             $(wildcard kernel/libs/ram/*.c) \
             $(wildcard kernel/libs/rtc64/*.c) \
             $(wildcard kernel/libs/usb/cherryusb/core/*.c) \
             kernel/libs/storage/ahci.c \
             kernel/libs/storage/atapi.c \
             kernel/libs/storage/iso9660.c \
             kernel/libs/storage/nvme.c \
             kernel/libs/storage/satapi.c \
             kernel/libs/storage/vdisk.c \
             kernel/libs/storage/vfs.c \
             $(wildcard kernel/libs/storage/fatfs/*.c) \
             $(wildcard kernel/libs/core/*.c) \
             programs/shell.c
AS_SRC = $(wildcard kernel/unice64/*.s)
KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(AS_SRC:.s=.o)
KERNEL_ELF = kernel.elf

# Standalone Programs (Flat RSL Binaries)
PROGRAMS = wm.bin editor.bin jit.bin gui_test.bin install.bin cargo.bin
NUKLEAR_PROGS = nk_demo.bin
PROG_LDFLAGS = -Wl,-T,programs/linker.ld -static -nostdlib

ISO_IMAGE = osx2.iso
SYSTEM_ISO = os2_system.iso
INSTALLER_ISO = os2_installer.iso
SATA_DISK = sata_disk.img
LIMINE_DIR = ./limine
LIMINE_BIN = $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin $(LIMINE_DIR)/limine-uefi-cd.bin

.PHONY: all menuconfig kernel programs iso run clean limine-setup system-iso installer-iso

all:
	$(MAKE) limine-setup
	$(MAKE) kernel
	$(MAKE) programs
	$(MAKE) system-iso
	$(MAKE) installer-iso
	@cp $(SYSTEM_ISO) $(ISO_IMAGE)
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

run: installer-iso $(SATA_DISK)
	qemu-system-x86_64 -M q35 -m 1G -serial stdio -cdrom $(INSTALLER_ISO) \
		-drive file=$(SATA_DISK),if=none,id=d0,format=raw \
		-device ich9-ahci,id=ahci \
		-device ide-hd,drive=d0,bus=ahci.0 \
		-nodefaults -vga std

$(SATA_DISK):
	@dd if=/dev/zero of=$(SATA_DISK) bs=1M count=64 status=none
	@echo "OSx2: 64MB Empty persistent disk created for internal installation test."

ramdisk.img:
	@python3 scripts/fat_tool.py ramdisk.img

kernel: limine-setup $(KERNEL_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(KERNEL_OBJ) -o $(KERNEL_ELF)
	@echo "OSx2 Limemade Kernel Compiled: $(KERNEL_ELF)"

programs: $(PROGRAMS) $(NUKLEAR_PROGS)

cargo.bin: programs/cargo.c programs/nuklear/nk_sovereign.h programs/libc/libc.c
	$(CC) $(CFLAGS) -DRSL_BINARY_MODE $(PROG_LDFLAGS) -Iprograms/nuklear -I. $< programs/libc/libc.c -o $@

nk_demo.bin: programs/nuklear/nk_demo.c programs/nuklear/nk_sovereign.h programs/libc/libc.c
	$(CC) $(CFLAGS) -DRSL_BINARY_MODE $(PROG_LDFLAGS) -Iprograms/nuklear -I. $< programs/libc/libc.c -o $@

%.bin: programs/%.c programs/libc/libc.c
	$(CC) $(CFLAGS) -DRSL_BINARY_MODE $(PROG_LDFLAGS) -I. $< programs/libc/libc.c -o $@

%.o: %.c | limine-setup
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s | limine-setup
	$(CC) $(CFLAGS) -c $< -o $@

system-iso: limine-setup kernel programs
	@rm -rf system_iso_root
	@mkdir -p system_iso_root/boot
	@mkdir -p system_iso_root/bin
	@mkdir -p system_iso_root/sys
	@cp $(KERNEL_ELF) system_iso_root/boot/
	@cp $(PROGRAMS) $(NUKLEAR_PROGS) system_iso_root/bin/
	@echo "echo Sovereign System Online." > system_iso_root/sys/boot.rsl
	@cp boot/limine.cfg system_iso_root/boot/
	@rm -rf iso_root
	@mkdir -p iso_root
	@cp -r system_iso_root/* iso_root/
	@python3 scripts/fat_tool.py ramdisk.img
	@cp ramdisk.img system_iso_root/boot/
	@if command -v xorriso >/dev/null 2>&1; then \
		cp $(LIMINE_BIN) system_iso_root/; \
		xorriso -as mkisofs -b limine-bios-cd.bin \
			-no-emul-boot -boot-load-size 4 -boot-info-table \
			--efi-boot limine-uefi-cd.bin \
			-efi-boot-part --efi-boot-image --protective-msdos-label \
			system_iso_root -o $(SYSTEM_ISO); \
		$(LIMINE_DIR)/limine bios-install $(SYSTEM_ISO); \
	else \
		touch $(SYSTEM_ISO); \
		echo "Warning: xorriso not found, created empty $(SYSTEM_ISO)."; \
	fi
	@echo "OSx2 System ISO Created: $(SYSTEM_ISO)"

installer-iso: limine-setup kernel programs system-iso
	@rm -rf installer_iso_root
	@mkdir -p installer_iso_root/boot
	@mkdir -p installer_iso_root/bin
	@mkdir -p installer_iso_root/CARGO
	@cp $(KERNEL_ELF) installer_iso_root/boot/
	@cp $(PROGRAMS) $(NUKLEAR_PROGS) installer_iso_root/bin/
	@cp $(SYSTEM_ISO) installer_iso_root/CARGO/os2_system.iso
	@cp installer_limine.cfg installer_iso_root/boot/limine.cfg
	@rm -rf iso_root
	@mkdir -p iso_root
	@cp -r installer_iso_root/* iso_root/
	@python3 scripts/fat_tool.py ramdisk.img
	@cp ramdisk.img installer_iso_root/boot/
	@if command -v xorriso >/dev/null 2>&1; then \
		cp $(LIMINE_BIN) installer_iso_root/; \
		xorriso -as mkisofs -b limine-bios-cd.bin \
			-no-emul-boot -boot-load-size 4 -boot-info-table \
			--efi-boot limine-uefi-cd.bin \
			-efi-boot-part --efi-boot-image --protective-msdos-label \
			installer_iso_root -o $(INSTALLER_ISO); \
		$(LIMINE_DIR)/limine bios-install $(INSTALLER_ISO); \
	else \
		touch $(INSTALLER_ISO); \
		echo "Warning: xorriso not found, created empty $(INSTALLER_ISO)."; \
	fi
	@echo "OSx2 Installer ISO Created: $(INSTALLER_ISO)"

iso: system-iso installer-iso
	@cp $(SYSTEM_ISO) $(ISO_IMAGE)

clean:
	rm -f $(KERNEL_OBJ) $(KERNEL_ELF) $(ISO_IMAGE) $(SYSTEM_ISO) $(INSTALLER_ISO) $(SATA_DISK) ramdisk.img $(PROGRAMS) programs/*.o
	rm -rf iso_root system_iso_root installer_iso_root
	@if [ -d "limine" ]; then $(MAKE) -C limine clean || true; fi
