#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* VDISK Node Structure */
typedef struct {
    uint32_t sector_size;
    uint64_t total_lba;
    void* private_data;
    int (*read_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*write_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    bool is_atapi;
} vdisk_node_t;

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
        /* Sovereign Handshake: Check for 0xEFBEADDE at LBA 0 */
        uint8_t sector[512];
        if (node.read_lba(node.private_data, 0, 1, sector) == 0) {
            uint32_t sig = *(uint32_t*)sector;
            if (sig == 0xEFBEADDE) {
                vga_print("[VDISK] Sovereign Signature Verified at LBA 0.\n");
            } else {
                vga_print("[VDISK] Warning: Raw Disk (No Sovereign Signature).\n");
            }
        }
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

int get_hw_disk_count(void) {
    return hw_count;
}

int get_connect_disk_count(void) {
    return connect_count;
}

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

#include <include/vfs.h>
void internal_rsl_ls(void* path);
void internal_rsl_cat(void* path);
void internal_rsl_write(void* path, void* content);
void internal_rsl_cd(void* path);
void internal_rsl_mkdir(void* path);
void internal_rsl_rmdir(void* path);
bool internal_rsl_exists(void* path);

void vdisk_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] /CONNECT registry (VDISK) initialized.\n");
        vfs_init();
        vfs_node_t root_node = {
            .name = "/",
            .ls = internal_rsl_ls,
            .cat = internal_rsl_cat,
            .write = internal_rsl_write,
            .cd = internal_rsl_cd,
            .mkdir = internal_rsl_mkdir,
            .rmdir = internal_rsl_rmdir,
            .exists = internal_rsl_exists
        };
        vfs_register_node(root_node);

        vfs_node_t connect_node = {
            .name = "/CONNECT",
            .ls = internal_rsl_ls,
            .exists = internal_rsl_exists
        };
        vfs_register_node(connect_node);
    }
}
