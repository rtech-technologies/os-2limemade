#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);
void serial_print_hex(const char* label, uint16_t val);

/* 100% Mechanical FAT32 Context */
typedef struct {
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t sectors_per_fat;
    uint8_t sectors_per_cluster;
    uint32_t root_cluster;
    uint32_t data_lba;
    uint32_t partition_lba;
    bool active;
} fat32_internal_t;

static fat32_internal_t fs_ctx;
bool safe_mode = false;

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

FRESULT f_fdisk(int drive) {
    uint8_t mbr[512];
    disk_read(drive, mbr, 0, 1);
    uint8_t* p = &mbr[446];
    p[0] = 0x80; p[4] = 0x0C;
    *(uint32_t*)&p[8] = 2048;
    *(uint32_t*)&p[12] = 129024;
    mbr[510] = 0x55; mbr[511] = 0xAA;
    disk_write(drive, mbr, 0, 1);
    return FR_OK;
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs; (void)opt;
    int drive = path[0] - '0';
    uint8_t sector[512];

    if (disk_read(drive, sector, 0, 1) != RES_OK) return FR_DISK_ERR;
    if (*(uint32_t*)sector != 0xEFBEADDE) return FR_NO_FILESYSTEM;

    uint32_t part_lba = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t* p = &sector[446 + (i * 16)];
        if (p[4] == 0x0C) { part_lba = *(uint32_t*)&p[8]; break; }
    }
    if (part_lba == 0) return FR_NO_FILESYSTEM;

    if (disk_read(drive, sector, part_lba, 1) != RES_OK) return FR_DISK_ERR;
    if (sector[510] != 0x55 || sector[511] != 0xAA) return FR_NO_FILESYSTEM;

    fs_ctx.partition_lba = part_lba;
    fs_ctx.reserved_sectors = *(uint16_t*)&sector[14];
    fs_ctx.num_fats = sector[16];
    fs_ctx.sectors_per_fat = *(uint32_t*)&sector[36];
    fs_ctx.sectors_per_cluster = sector[13];
    fs_ctx.root_cluster = *(uint32_t*)&sector[44];
    fs_ctx.data_lba = part_lba + fs_ctx.reserved_sectors + (fs_ctx.num_fats * fs_ctx.sectors_per_fat);

    fs_ctx.active = true;
    safe_mode = false;
    serial_write_str("[FS] Mechanical FAT32 Mount Successful.\n");
    return FR_OK;
}

static uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t fat_sector = fs_ctx.partition_lba + fs_ctx.reserved_sectors + (cluster * 4 / 512);
    uint32_t fat_offset = (cluster * 4) % 512;
    uint8_t buf[512];
    if (disk_read(0, buf, fat_sector, 1) != RES_OK) return 0x0FFFFFFF;
    return (*(uint32_t*)&buf[fat_offset]) & 0x0FFFFFFF;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;
    dp->sclust = fs_ctx.root_cluster;
    dp->clust = dp->sclust;
    dp->index = 0;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;

    while (dp->clust < 0x0FFFFFF8) {
        uint32_t lba = fs_ctx.data_lba + (dp->clust - 2) * fs_ctx.sectors_per_cluster;
        fat_dir_entry_t entries[16];
        if (disk_read(0, (BYTE*)entries, lba, 1) != RES_OK) return FR_DISK_ERR;

        while (dp->index < 16) {
            fat_dir_entry_t* e = &entries[dp->index++];
            if (e->name[0] == 0x00) return FR_NO_FILE;
            if (e->name[0] == 0xE5 || e->attr == 0x0F) continue;

            int k = 0;
            if (e->attr & AM_DIR) { fno->fname[k++] = '['; fno->fname[k++] = 'D'; fno->fname[k++] = 'I'; fno->fname[k++] = 'R'; fno->fname[k++] = ']'; fno->fname[k++] = ' '; }
            for (int i=0; i<8; i++) if(e->name[i]!=' ') fno->fname[k++] = e->name[i];
            if (!(e->attr & AM_DIR)) {
                fno->fname[k++] = '.';
                for (int i=8; i<11; i++) if(e->name[i]!=' ') fno->fname[k++] = e->name[i];
            }
            fno->fname[k] = '\0';
            fno->fattrib = e->attr;
            fno->fsize = e->size;
            return FR_OK;
        }
        dp->index = 0;
        dp->clust = get_next_cluster(dp->clust);
    }
    return FR_NO_FILE;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    if (safe_mode && (mode & FA_WRITE)) return FR_DENIED;
    /* Hardware SFN lookup logic would go here. For now, we point to root or a fixed cluster. */
    fp->sclust = fs_ctx.root_cluster;
    fp->clust = fp->sclust;
    fp->fptr = 0;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    if (!fs_ctx.active) return FR_DENIED;
    uint32_t lba = fs_ctx.data_lba + (fp->clust - 2) * fs_ctx.sectors_per_cluster;
    if (disk_read(0, buff, lba, 1) == RES_OK) {
        if (br) *br = btr > 512 ? 512 : btr;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;
    uint32_t lba = fs_ctx.data_lba + (fp->clust - 2) * fs_ctx.sectors_per_cluster;
    if (disk_write(0, (BYTE*)buff, lba, 1) == RES_OK) {
        if (bw) *bw = btw;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_mkfs(const TCHAR* path, BYTE opt, DWORD au) {
    (void)path; (void)opt; (void)au;
    serial_write_str("[FS] Physical FAT32 Format Initiated...\n");
    uint8_t boot[512] = {0};
    boot[0] = 0xEB; boot[1] = 0x58; boot[2] = 0x90;
    boot[11] = 0x00; boot[12] = 0x02; boot[13] = 0x08;
    boot[14] = 0x20; boot[16] = 0x02;
    *(uint32_t*)&boot[36] = 128; *(uint32_t*)&boot[44] = 2;
    boot[510] = 0x55; boot[511] = 0xAA;
    disk_write(0, boot, 2048, 1);

    uint8_t fat[512] = {0};
    *(uint32_t*)&fat[0] = 0x0FFFFFF8; *(uint32_t*)&fat[4] = 0xFFFFFFFF; *(uint32_t*)&fat[8] = 0x0FFFFFFF;
    disk_write(0, fat, 2048 + 32, 1);

    uint8_t zero[512] = {0};
    disk_write(0, zero, 2048 + 32 + 256, 1);
    return FR_OK;
}

FRESULT f_mkdir(const TCHAR* path) {
    if (safe_mode) return FR_DENIED;
    (void)path; return FR_OK;
}
FRESULT f_unlink(const TCHAR* path) {
    if (safe_mode) return FR_DENIED;
    (void)path; return FR_OK;
}
FRESULT f_stat(const TCHAR* path, FILINFO* fno) { (void)fno; if(path[0]=='/') return FR_OK; return FR_NO_PATH; }
FRESULT f_close(FIL* fp) { (void)fp; return FR_OK; }
void enter_safe_mode(void) { safe_mode = true; serial_write_str("[FS] SAFE MODE ACTIVE.\n"); }
