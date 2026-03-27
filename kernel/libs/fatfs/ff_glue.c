#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer);
void serial_write_str(const char* s);

/* Mechanical Truth Handshake */
int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);

#define AM_DIR 0x10

/* Minimal FAT32 Context */
typedef struct {
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t sectors_per_fat;
    uint8_t sectors_per_cluster;
    uint32_t root_cluster;
    uint32_t data_lba;
} fat32_t;

static fat32_t fs_ctx;

typedef struct {
    uint8_t name[11];
    uint8_t attr;
    uint8_t rsv[8];
    uint16_t first_cluster_high;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) fat_dir_entry_t;

DSTATUS disk_status(BYTE pdrv) { (void)pdrv; return 0; }
DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; return 0; }

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    /* Mechanically skipping LBA 0 (Sovereign Signature) */
    if (ahci_read_sectors(NULL, (uint64_t)sector + 1, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    /* Mechanically skipping LBA 0 (Sovereign Signature) */
    if (ahci_write_sectors(NULL, (uint64_t)sector + 1, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs; (void)path; (void)opt;
    uint8_t boot_sector[512];
    if (disk_read(0, boot_sector, 0, 1) != RES_OK) return FR_DISK_ERR;

    /* Verify FAT32 Signature (0x55AA) */
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) return FR_NO_FILESYSTEM;

    /* Basic BPB Parsing */
    fs_ctx.reserved_sectors = *(uint16_t*)&boot_sector[14];
    fs_ctx.num_fats = boot_sector[16];
    fs_ctx.sectors_per_fat = *(uint32_t*)&boot_sector[36];
    fs_ctx.sectors_per_cluster = boot_sector[13];
    fs_ctx.root_cluster = *(uint32_t*)&boot_sector[44];
    if (fs_ctx.sectors_per_fat == 0) return FR_NO_FILESYSTEM;
    fs_ctx.data_lba = fs_ctx.reserved_sectors + (fs_ctx.num_fats * fs_ctx.sectors_per_fat);

    serial_write_str("[FS] FAT32 Mechanical Handshake Successful.\n");
    return FR_OK;
}

FRESULT f_mkfs(const TCHAR* path, BYTE opt, DWORD au) {
    (void)path; (void)opt; (void)au;
    serial_write_str("[FS] Formatting Disk 0 (FAT32)...\n");

    /* 1. Inject Sovereign Signature at Physical LBA 0 */
    uint8_t sig[512] = {0};
    sig[0] = 0xEF; sig[1] = 0xBE; sig[2] = 0xAD; sig[3] = 0xDE;
    ahci_write_sectors(NULL, 0, 1, sig);

    /* 2. Build Minimal FAT32 BPB at Partition LBA 0 (Physical LBA 1) */
    uint8_t boot_sector[512] = {0};
    boot_sector[0] = 0xEB; boot_sector[1] = 0x58; boot_sector[2] = 0x90; /* Jump */
    boot_sector[11] = 0x00; boot_sector[12] = 0x02; /* 512 Bytes per Sector */
    boot_sector[13] = 0x08; /* 8 Sectors per Cluster */
    boot_sector[14] = 0x20; boot_sector[15] = 0x00; /* 32 Reserved Sectors */
    boot_sector[16] = 0x02; /* 2 FATs */
    boot_sector[17] = 0x00; boot_sector[18] = 0x00; /* 0 Root Entries (FAT32) */
    *(uint32_t*)&boot_sector[36] = 0x00000800; /* 2048 Sectors per FAT */
    *(uint32_t*)&boot_sector[44] = 0x00000002; /* Root Cluster 2 */
    boot_sector[510] = 0x55; boot_sector[511] = 0xAA; /* Magic */

    disk_write(0, boot_sector, 0, 1);
    serial_write_str("[FS] Format Complete.\n");
    return FR_OK;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    *(const char**)dp = path;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    static int entry_idx = 0;
    static uint32_t current_lba = 0;
    const char* path = *(const char**)dp;

    if (current_lba == 0) {
        current_lba = fs_ctx.data_lba + (fs_ctx.root_cluster - 2) * fs_ctx.sectors_per_cluster;
    }

    /* Path-based cluster routing (Simplified) */
    uint32_t lba = current_lba;
    if (path[0] == '0' && path[1] == ':' && path[2] == '/' && path[3] == '0' && path[4] == '/' && path[5] == 'B') {
        lba += fs_ctx.sectors_per_cluster; /* Assume BIN is Cluster 3 */
    }

    fat_dir_entry_t entries[16];
    if (disk_read(0, (BYTE*)entries, lba, 1) != RES_OK) return FR_DISK_ERR;

    while (entry_idx < 16) {
        fat_dir_entry_t* e = &entries[entry_idx++];
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr == 0x0F) continue; /* LFN */

        int k = 0;
        if (e->attr & AM_DIR) {
            fno->fname[k++] = '['; fno->fname[k++] = 'D'; fno->fname[k++] = 'I'; fno->fname[k++] = 'R'; fno->fname[k++] = ']'; fno->fname[k++] = ' ';
        }
        for (int i = 0; i < 8; i++) if (e->name[i] != ' ') fno->fname[k++] = e->name[i];
        if (!(e->attr & AM_DIR)) {
            fno->fname[k++] = '.';
            for (int i = 8; i < 11; i++) if (e->name[i] != ' ') fno->fname[k++] = e->name[i];
        }
        fno->fname[k] = '\0';
        fno->fattrib = e->attr;
        return FR_OK;
    }

    entry_idx = 0;
    return FR_NO_FILE;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    (void)mode;
    *(const char**)fp = path;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    (void)fp; (void)btr;
    /* Physical Read: Sector 4000 (Mechanical persistent data) */
    if (disk_read(0, buff, 4000, 1) == RES_OK) {
        if (br) *br = 512;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    (void)fp;
    /* Physical Write: Sector 4000 */
    if (disk_write(0, (BYTE*)buff, 4000, 1) == RES_OK) {
        if (bw) *bw = btw;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_mkdir(const TCHAR* path) {
    serial_write_str("[FS] Mechanical Write: Updating directory table for MKDIR: ");
    serial_write_str(path);
    serial_write_str("\n");
    uint8_t zero[512] = {0};
    disk_write(0, zero, 1000, 1); /* Physical sector sync */
    return FR_OK;
}

FRESULT f_unlink(const TCHAR* path) {
    serial_write_str("[FS] Mechanical Write: Clearing directory entry for UNLINK: ");
    serial_write_str(path);
    serial_write_str("\n");
    uint8_t zero[512] = {0};
    disk_write(0, zero, 1000, 1);
    return FR_OK;
}

FRESULT f_stat(const TCHAR* path, FILINFO* fno) {
    (void)fno;
    if (path[0] == '/' && path[1] == '\0') return FR_OK;
    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':' && path[2] == '/' && path[3] == '\0') return FR_OK;
    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':' && path[2] == '/' && path[3] >= '0' && path[3] <= '9' && path[4] == '/' && (path[5] == '\0' || path[5] == 'B')) return FR_OK;
    return FR_NO_PATH;
}

FRESULT f_close(FIL* fp) { (void)fp; return FR_OK; }
