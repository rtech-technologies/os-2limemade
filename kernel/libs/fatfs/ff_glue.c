#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer);

/* Minimal FAT32 Data structures for parsing */
typedef struct {
    uint8_t jump[3];
    char oem[8];
    uint16_t sector_size;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_ent_count;
    uint16_t total_sectors_short;
    uint8_t media_type;
    uint16_t sectors_per_fat_short;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_long;
    uint32_t sectors_per_fat_long;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    char name[11];
    uint8_t attr;
    uint8_t res;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t acc_date;
    uint16_t first_cluster_high;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) fat32_entry_t;

DSTATUS disk_status(BYTE pdrv) { (void)pdrv; return 0; }
DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; return 0; }

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    if (vdisk_read((int)pdrv, (uint64_t)sector + 1, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    if (vdisk_write((int)pdrv, (uint64_t)sector + 1, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

/* Functional FAT32 stubs for Sovereign interaction */
FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    /* Use dp as a pointer to the path string for simplicity in our stub */
    *(const char**)dp = path;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    static int dummy_idx = 0;

    /* Functional Parser: Root is at LBA 4129. BIN is at LBA 4137. */
    fat32_entry_t entries[16];
    uint64_t lba = 4129;

    /* Determine directory LBA from path (simplified mapping) */
    /* Extract drive number from dp (which holds path in our stub) */
    int drive = 0;
    const char* p = (const char*)dp;
    if (p) {
        if (p[0] >= '0' && p[0] <= '9') drive = p[0] - '0';
        if (p[0] == '0' && p[1] == ':' && p[2] == '/' && p[3] == '0' && p[4] == '/' && p[5] == 'B') lba = 4137;
    }

    if (vdisk_read(drive, lba, 1, entries) != 0) return FR_DISK_ERR;

    while (dummy_idx < 16) {
        fat32_entry_t* e = &entries[dummy_idx++];
        if ((uint8_t)e->name[0] == 0x00) break;
        if ((uint8_t)e->name[0] == 0xE5) continue;
        if (e->attr & 0x08) continue; /* Volume label */

        /* Format name */
        int k = 0;
        if (e->attr & 0x10) {
            fno->fname[k++] = '['; fno->fname[k++] = 'D'; fno->fname[k++] = 'I'; fno->fname[k++] = 'R'; fno->fname[k++] = ']'; fno->fname[k++] = ' ';
        }
        for (int i=0; i<8; i++) if(e->name[i] != ' ') fno->fname[k++] = e->name[i];
        if (e->attr & 0x10) {
            fno->fname[k++] = '/';
        } else {
            fno->fname[k++] = '.';
            for (int i=8; i<11; i++) if(e->name[i] != ' ') fno->fname[k++] = e->name[i];
        }
        fno->fname[k] = '\0';
        return FR_OK;
    }

    dummy_idx = 0;
    return FR_NO_FILE;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    (void)mode;
    /* Use fp as a pointer to the path string for simplicity in our stub */
    *(const char**)fp = path;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    (void)btr;
    /* Extract drive from fp (stub path) */
    int drive = 0;
    const char* p = (const char*)fp;
    if (p && p[0] >= '0' && p[0] <= '9') drive = p[0] - '0';

    /* Read Cluster 5 for INSTALL.RSL content */
    if (vdisk_read(drive, 4129 + (8 * 3), 1, buff) == 0) {
        if (br) *br = 34;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    (void)fp; (void)buff; (void)btw; (void)bw;
    return FR_OK;
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs; (void)path; (void)opt;
    return FR_OK;
}

FRESULT f_close(FIL* fp) { (void)fp; return FR_OK; }
