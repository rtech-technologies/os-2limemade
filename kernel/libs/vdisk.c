#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>

/* VDISK Node Structure */
typedef struct {
    uint32_t sector_size;
    uint64_t total_lba;
    void* private_data;
    int (*read_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*write_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
} vdisk_node_t;

#define MAX_VDISKS 8
static vdisk_node_t vdisk_registry[MAX_VDISKS];
static int vdisk_count = 0;

void serial_write_str(const char* s);

void register_vdisk(vdisk_node_t node) {
    if (vdisk_count < MAX_VDISKS) {
        vdisk_registry[vdisk_count++] = node;
        serial_write_str("[VDISK] Registered new OSx2 Limemade disk to /CONNECT.\n");
    }
}

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer) {
    if (disk_id < 0 || disk_id >= vdisk_count) return -1;
    return vdisk_registry[disk_id].read_lba(vdisk_registry[disk_id].private_data, lba, count, buffer);
}

int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer) {
    if (disk_id < 0 || disk_id >= vdisk_count) return -1;
    if (!vdisk_registry[disk_id].write_lba) return -1;
    return vdisk_registry[disk_id].write_lba(vdisk_registry[disk_id].private_data, lba, count, buffer);
}

int get_vdisk_count(void) {
    return vdisk_count;
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
    }
}
