#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <include/string.h>
#include <include/stdlib.h>
#include <stdbool.h>
#include <limine.h>
#include <include/ahci_hw.h>

#include <kernel/libs/storage/vdisk.h>

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

static bool str_match_local(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

void register_hardware_disk(vdisk_node_t node) {
    if (hw_count < MAX_DISKS) {
        /* OSx2 Priority Shift: Prioritize SATA_HDD over others */
        if (str_match_local(node.name, "SATA_HDD") && hw_count > 0) {
            /* Shift existing nodes down to insert SATA_HDD at front */
            for (int i = hw_count; i > 0; i--) {
                hw_registry[i] = hw_registry[i-1];
            }
            hw_registry[0] = node;
            hw_count++;
        } else {
            hw_registry[hw_count++] = node;
        }
        vga_print("[VDISK] Physical hardware registered: %s\n", node.name);
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

int get_hw_disk_id_by_name(const char* name) {
    for (int i = 0; i < hw_count; i++) {
        if (str_match_local(hw_registry[i].name, name)) return i;
    }
    return -1;
}

#include <include/stdlib.h>

int is_sovereign_disk(int disk_id) {
    if (disk_id < 0 || disk_id >= hw_count) return 0;
    uint8_t* buf = malloc(512);
    if (!buf) return 0;

    int res = 0;
    if (hw_registry[disk_id].read_lba(hw_registry[disk_id].private_data, 0, 1, buf) == 0) {
        uint32_t sig = *(uint32_t*)buf;
        if (sig == 0xEFBEADDE) res = 1;
    }
    free(buf);
    return res;
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

int vdisk_eject_hw(int hw_id) {
    if (hw_id < 0 || hw_id >= hw_count) return -1;
    if (!hw_registry[hw_id].eject) return -1;
    return hw_registry[hw_id].eject(hw_registry[hw_id].private_data);
}

bool vdisk_is_busy(int hw_id) {
    if (hw_id < 0 || hw_id >= hw_count) return false;

    /* Hardware Status Check: AHCI Port Busy bit (BSY = bit 7 of TFD) */
    void* get_hba_base(void);
    hba_mem_t* hba = (hba_mem_t*)get_hba_base();
    if (hba && !hw_registry[hw_id].is_atapi) {
        int port = (int)(uint64_t)hw_registry[hw_id].private_data;
        if (port < 32 && (hba->ports[port].tfd & 0x80)) return true;
    }

    return false;
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

#include <include/string.h>

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
    memcpy(buffer, src, size);
    return 0;
}

#include <include/vfs.h>
#include <kernel/libs/storage/fatfs/ff.h>

static void vdisk_ls_root(void* path, void* priv) {
    (void)path; (void)priv;
    int count = get_hw_disk_count();
    for (int i = 0; i < count; i++) {
        char buf[128];
        int k = 0;
        buf[k++] = '0' + i; buf[k++] = ':'; buf[k++] = '/'; buf[k++] = ' ';

        /* Display Hardware Label */
        const char* label = hw_registry[i].name;
        if (label[0]) {
            buf[k++] = '(';
            while(*label) buf[k++] = *label++;
            buf[k++] = ')'; buf[k++] = ' ';
        }

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
                .name = "RAMDISK",
                .sector_size = 512,
                .total_lba = resp->modules[0]->size / 512,
                .partition_offset = 2048, /* Sovereign Partition Standard */
                .read_lba = ramdisk_read,
                .write_lba = NULL,
                .is_atapi = false /* RAMDISK is virtual, not ATAPI */
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
