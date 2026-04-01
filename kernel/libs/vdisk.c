#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "vdisk.h"

#define MAX_DISKS 16

/* Physical Hardware Registry */
static vdisk_node_t hw_registry[MAX_DISKS];
static int hw_count = 0;

/* Logical Connected Registry (/CONNECT) */
static vdisk_node_t connect_registry[MAX_DISKS];
static int connect_count = 0;

void serial_write_str(const char* s);
void vga_print(const char* fmt, ...);
struct limine_module_response* get_modules(void);

void register_hardware_disk(vdisk_node_t node) {
    if (hw_count < MAX_DISKS) {
        hw_registry[hw_count++] = node;
        vga_print("[VDISK] Physical hardware registered.\n");
    }
}

void vdisk_connect(int hw_id) {
    if (hw_id >= 0 && hw_id < hw_count && connect_count < MAX_DISKS) {
        connect_registry[connect_count++] = hw_registry[hw_id];
        serial_write_str("[CONNECT] Disk volume linked.\n");
    }
}

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer) {
    if (disk_id < 0 || disk_id >= hw_count) return -1;
    return hw_registry[disk_id].read_lba(hw_registry[disk_id].private_data, lba, count, buffer);
}

int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer) {
    if (disk_id < 0 || disk_id >= hw_count) return -1;
    if (!hw_registry[disk_id].write_lba) return -1;
    return hw_registry[disk_id].write_lba(hw_registry[disk_id].private_data, lba, count, buffer);
}

int get_hw_disk_count(void) { return hw_count; }
int get_connect_disk_count(void) { return connect_count; }

int vdisk_read_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer) {
    if (hw_id < 0 || hw_id >= hw_count) return -1;
    return hw_registry[hw_id].read_lba(hw_registry[hw_id].private_data, lba, count, buffer);
}

int vdisk_write_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer) {
    if (hw_id < 0 || hw_id >= hw_count) return -1;
    if (!hw_registry[hw_id].write_lba) return -1;
    return hw_registry[hw_id].write_lba(hw_registry[hw_id].private_data, lba, count, buffer);
}

uint64_t vdisk_get_offset(int hw_id) {
    if (hw_id < 0 || hw_id >= hw_count) return 0;
    return hw_registry[hw_id].partition_offset;
}

bool vdisk_is_readonly(int hw_id) {
    if (hw_id < 0 || hw_id >= hw_count) return true;
    return hw_registry[hw_id].write_lba == NULL;
}

bool vdisk_is_atapi(int hw_id) {
    if (hw_id < 0 || hw_id >= hw_count) return false;
    return hw_registry[hw_id].is_atapi;
}

static int ramdisk_read(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    (void)priv;
    struct limine_module_response* resp = get_modules();
    if (!resp || resp->module_count == 0) return -1;
    struct limine_file* ramdisk = resp->modules[0];
    uint8_t* base = (uint8_t*)ramdisk->address;
    size_t offset = lba * 512;
    size_t size = count * 512;
    if (offset + size > ramdisk->size) return -1;
    uint8_t* src = base + offset;
    uint8_t* dst = (uint8_t*)buffer;
    for (size_t i = 0; i < size; i++) dst[i] = src[i];
    return 0;
}

#include <include/vfs.h>
#include <kernel/libs/fatfs/ff.h>

static void vdisk_ls_root(void* path, void* priv) {
    (void)path; (void)priv;
    int count = get_hw_disk_count();
    for (int i = 0; i < count; i++) {
        char buf[64];
        int k = 0;
        buf[k++] = '0' + i; buf[k++] = ':'; buf[k++] = '/'; buf[k++] = ' ';

        FATFS tmp;
        if (f_mount(&tmp, i) == FR_OK) {
            const char* tag = "[SOVEREIGN]";
            while(*tag) buf[k++] = *tag++;
        } else {
            const char* tag = "[RAW DISK]";
            while(*tag) buf[k++] = *tag++;
        }
        if (vdisk_is_atapi(i)) {
            const char* tag = " (ATAPI)";
            while(*tag) buf[k++] = *tag++;
        }
        if (!hw_registry[i].write_lba) {
            const char* tag = " [READ ONLY]";
            while(*tag) buf[k++] = *tag++;
        }
        buf[k++] = '\n'; buf[k++] = '\0';
        print(buf);
    }
}

void vdisk_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] VDISK Registry initialized.\n");
        vfs_init();

        /* Register INITRD if module present */
        struct limine_module_response* resp = get_modules();
        if (resp && resp->module_count > 0) {
            vdisk_node_t initrd = {
                .sector_size = 512,
                .total_lba = resp->modules[0]->size / 512,
                .partition_offset = 2048, /* Sovereign Partition Standard */
                .read_lba = ramdisk_read,
                .write_lba = NULL,
                .is_atapi = true /* Label it as ATAPI for main.c identification */
            };
            register_hardware_disk(initrd);
            serial_write_str("[INIT] Ramdisk registered as Physical Volume.\n");
        }

        vfs_node_t root_node = {
            .name = "/",
            .ls = vdisk_ls_root
        };
        vfs_register_node(root_node);
    }
}
