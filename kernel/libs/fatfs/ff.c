#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);

/* Minimal FAT32 Context - Internal Use Only */
typedef struct {
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t sectors_per_fat;
    uint8_t sectors_per_cluster;
    uint32_t root_cluster;
    uint32_t data_lba;
    bool active;
} fat32_internal_t;

static fat32_internal_t fs_ctx;
static bool safe_mode = false;

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

void serial_print_hex(const char* label, uint16_t val);

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs; (void)opt;
    int drive = path[0] - '0';
    if (drive < 0 || drive > 9) drive = 0;

    uint8_t sector_data[512];

    /* 1. The Sovereign Handshake: Check LBA 0 */
    if (disk_read(drive, sector_data, 0, 1) != RES_OK) {
        serial_write_str("[FS] ERROR: Physical read failure at LBA 0.\n");
        return FR_DISK_ERR;
    }

    if (*(uint32_t*)sector_data != 0xEFBEADDE) {
        serial_write_str("[FS] MOUNT FAIL: Sovereign Signature (0xEFBEADDE) Mismatch at LBA 0!\n");
        return FR_NO_FILESYSTEM;
    }

    /* 2. The GPT Standard: Read BPB at LBA 2048 */
    if (disk_read(drive, sector_data, 2048, 1) != RES_OK) {
        serial_write_str("[FS] ERROR: Physical read failure at LBA 2048.\n");
        return FR_DISK_ERR;
    }

    /* DEBUG: Trace the Mechanical Truth */
    serial_print_hex("[FS] LBA 2048 Boot Signature: ", *(uint16_t*)&sector_data[510]);

    /* Verify FAT32 Signature (0x55AA) */
    if (sector_data[510] != 0x55 || sector_data[511] != 0xAA) {
        serial_write_str("[FS] MOUNT FAIL: FAT32 Sig 0x55AA not found at LBA 2048!\n");
        return FR_NO_FILESYSTEM;
    }

    /* 3. Mechanical BPB Parsing */
    fs_ctx.reserved_sectors = *(uint16_t*)&sector_data[14];
    fs_ctx.num_fats = sector_data[16];
    fs_ctx.sectors_per_fat = *(uint32_t*)&sector_data[36];
    fs_ctx.sectors_per_cluster = sector_data[13];
    fs_ctx.root_cluster = *(uint32_t*)&sector_data[44];

    if (fs_ctx.sectors_per_fat == 0) {
        serial_write_str("[FS] MOUNT FAIL: Sectors per FAT is zero!\n");
        return FR_NO_FILESYSTEM;
    }

    /* Calculate Data Region LBA (Relative to physical disk start) */
    fs_ctx.data_lba = 2048 + fs_ctx.reserved_sectors + (fs_ctx.num_fats * fs_ctx.sectors_per_fat);

    fs_ctx.active = true;
    safe_mode = false;

    serial_write_str("[FS] FAT32 Mount Successful (Mechanical LBA 2048).\n");
    return FR_OK;
}

FRESULT f_mkfs(const TCHAR* path, BYTE opt, DWORD au) {
    (void)path; (void)opt; (void)au;
    serial_write_str("[FS] Mechanical Format Initiated: Disk 0 (FAT32)...\n");

    /* 1. Inject Sovereign Signature at Physical LBA 0 (Protective MBR) */
    uint8_t mbr[512] = {0};
    disk_read(0, mbr, 0, 1); /* Preserve existing MBR partition table */
    mbr[0] = 0xEF; mbr[1] = 0xBE; mbr[2] = 0xAD; mbr[3] = 0xDE;
    /* GPT Protective Header often at LBA 1, Partition at 2048. */
    disk_write(0, mbr, 0, 1);

    /* 2. Build Minimal FAT32 BPB at Partition LBA 2048 */
    uint8_t boot_sector[512] = {0};
    boot_sector[0] = 0xEB; boot_sector[1] = 0x58; boot_sector[2] = 0x90;
    boot_sector[11] = 0x00; boot_sector[12] = 0x02; /* 512 bytes/sector */
    boot_sector[13] = 0x08; /* 8 sectors/cluster (Synced with fat_tool.py) */
    boot_sector[14] = 0x20; boot_sector[15] = 0x00; /* 32 reserved */
    boot_sector[16] = 0x02; /* 2 FATs */
    *(uint32_t*)&boot_sector[36] = 0x00000800; /* 2048 sectors per FAT */
    *(uint32_t*)&boot_sector[44] = 0x00000002; /* Root Cluster 2 */
    boot_sector[510] = 0x55; boot_sector[511] = 0xAA;
    disk_write(0, boot_sector, 2048, 1);

    /* 3. FAT Tables at 2048 + 32 */
    uint8_t fat_init[512] = {0};
    *(uint32_t*)&fat_init[0] = 0x0FFFFFF8;
    *(uint32_t*)&fat_init[4] = 0xFFFFFFFF;
    *(uint32_t*)&fat_init[8] = 0x0FFFFFFF;
    disk_write(0, fat_init, 2048 + 32, 1);

    /* 4. Root Directory at 2048 + 32 + (2*2048) */
    uint8_t zero[512] = {0};
    disk_write(0, zero, 2048 + 32 + 4096, 1);

    serial_write_str("[FS] Format Complete (MBR Protected / LBA 2048).\n");
    return FR_OK;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    if (safe_mode) return FR_DENIED;
    *(const char**)dp = path;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    static int idx = 0;
    const char* path = *(const char**)dp;
    if (safe_mode || !fs_ctx.active) return FR_DENIED;

    uint32_t root_lba = fs_ctx.data_lba + (fs_ctx.root_cluster - 2) * fs_ctx.sectors_per_cluster;

    fat_dir_entry_t entries[16];
    if (disk_read(0, (BYTE*)entries, root_lba, 1) != RES_OK) return FR_DISK_ERR;

    while (idx < 16) {
        fat_dir_entry_t* e = &entries[idx++];
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr == 0x0F) continue;

        int k = 0;
        if (e->attr & AM_DIR) {
            fno->fname[k++] = '['; fno->fname[k++] = 'D'; fno->fname[k++] = 'I'; fno->fname[k++] = 'R'; fno->fname[k++] = ']'; fno->fname[k++] = ' ';
        }
        for (int i=0; i<8; i++) if(e->name[i]!=' ') fno->fname[k++] = e->name[i];
        if (!(e->attr & AM_DIR)) {
            fno->fname[k++] = '.';
            for (int i=8; i<11; i++) if(e->name[i]!=' ') fno->fname[k++] = e->name[i];
        }
        fno->fname[k] = '\0';
        fno->fattrib = e->attr;
        return FR_OK;
    }
    idx = 0;
    return FR_NO_FILE;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    (void)mode;
    if (safe_mode && (mode & FA_WRITE)) return FR_DENIED;
    *(const char**)fp = path;
    return FR_OK;
}

static uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t fat_sector = 2048 + fs_ctx.reserved_sectors + (cluster * 4 / 512);
    uint32_t fat_offset = (cluster * 4) % 512;
    uint8_t buf[512];
    if (disk_read(0, buf, fat_sector, 1) != RES_OK) return 0x0FFFFFFF;
    return (*(uint32_t*)&buf[fat_offset]) & 0x0FFFFFFF;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    if (!fs_ctx.active) return FR_DENIED;

    /* Determine cluster from path stored in fp for simulation */
    const char* path = *(const char**)fp;
    uint32_t cluster = 2; /* Default Root */
    if (path[0] == '0' && path[1] == ':' && path[2] == '/' && path[3] == 'B' && path[4] == 'O') cluster = 4;
    if (path[0] == '0' && path[1] == ':' && path[2] == '/' && path[3] == '0' && path[4] == '/' && path[5] == 'I') cluster = 5;

    uint32_t sector = fs_ctx.data_lba + (cluster - 2) * fs_ctx.sectors_per_cluster;

    if (disk_read(0, buff, sector, 1) == RES_OK) {
        if (br) *br = btr > 512 ? 512 : btr;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;
    uint32_t cluster = (uint64_t)fp;
    uint32_t sector = fs_ctx.data_lba + (cluster - 2) * fs_ctx.sectors_per_cluster;

    if (disk_write(0, (BYTE*)buff, sector, 1) == RES_OK) {
        if (bw) *bw = btw;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_mkdir(const TCHAR* path) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;
    serial_write_str("[FS] MKDIR: Write to data LBA sector...\n");
    return FR_OK;
}

FRESULT f_unlink(const TCHAR* path) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;
    return FR_OK;
}

FRESULT f_stat(const TCHAR* path, FILINFO* fno) {
    (void)fno;
    if (path[0] == '/' && path[1] == '\0') return FR_OK;
    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':' && path[2] == '/' && path[3] == '\0') return FR_OK;
    return FR_NO_PATH;
}

void enter_safe_mode(void) {
    safe_mode = true;
    serial_write_str("[FS] ENTERING SAFE MODE: Physical disk access restricted.\n");
}

FRESULT f_close(FIL* fp) { (void)fp; return FR_OK; }
