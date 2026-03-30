#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

void register_hardware_disk(vdisk_node_t node) {
    if (hw_count < MAX_DISKS) {
        hw_registry[hw_count++] = node;
        vga_print("[VDISK] Physical hardware detected and registered.\n");
    }
}

void vdisk_connect(int hw_id) {
    if (hw_id >= 0 && hw_id < hw_count && connect_count < MAX_DISKS) {
        connect_registry[connect_count++] = hw_registry[hw_id];
        serial_write_str("[CONNECT] Disk volume linked to Sovereign /CONNECT registry.\n");
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

bool vdisk_is_atapi(int hw_id) {
    if (hw_id < 0 || hw_id >= hw_count) return false;
    return hw_registry[hw_id].is_atapi;
}

int is_sovereign_disk(int disk_id) {
    uint32_t buffer[128]; /* 512 bytes */
    if (vdisk_read_hw(disk_id, 0, 1, buffer) != 0) return 0;
    if (buffer[0] == 0xEFBEADDE) {
        vga_print("[VDISK] OSx2 Limemade signature 0xEFBEADDE found!\n");
        return 1;
    }
    return 0;
}

#include <include/vfs.h>

void vdisk_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] /CONNECT registry (VDISK) initialized.\n");
        vfs_init();
    }
}
