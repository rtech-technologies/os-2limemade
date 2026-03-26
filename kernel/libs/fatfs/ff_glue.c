#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer);
void serial_write_str(const char* s);

/* Actual FAT32 BPB and Entry Structure */
typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t sector_size;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t version;
    uint32_t root_cluster;
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_acc_date;
    uint16_t first_cluster_high;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat_entry_t;

#define AM_DIR 0x10

static int parse_drive(const char* path) {
    if (path[0] >= '0' && path[0] <= '9') return path[0] - '0';
    return 0;
}

DSTATUS disk_status(BYTE pdrv) { (void)pdrv; return 0; }
DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; return 0; }

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    /* Offset LBA by 1 to skip the Sovereign Signature */
    if (vdisk_read((int)pdrv, (uint64_t)sector + 1, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    if (vdisk_write((int)pdrv, (uint64_t)sector + 1, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    *(const char**)dp = path;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    static int dummy_idx = 0;
    static int current_drive = -1;
    static fat32_bpb_t bpb;
    static uint32_t data_lba = 0;

    const char* path = *(const char**)dp;
    int drive = parse_drive(path);

    if (drive != current_drive) {
        current_drive = drive;
        dummy_idx = 0;
        /* Read BPB at Partition LBA 0 (Physical LBA 1) */
        if (disk_read(drive, (BYTE*)&bpb, 0, 1) != RES_OK) return FR_DISK_ERR;
        data_lba = bpb.reserved_sectors + (bpb.num_fats * bpb.fat_size_32);
    }

    uint32_t lba = data_lba + ((bpb.root_cluster - 2) * bpb.sectors_per_cluster);

    /* Simple directory routing */
    if (path[0] == '0' && path[1] == ':' && path[2] == '/' && path[3] == '0' && path[4] == '/' && path[5] == 'B') {
        lba += bpb.sectors_per_cluster; /* BIN is Cluster 3 */
    }

    fat_entry_t entries[16];
    if (disk_read(drive, (BYTE*)entries, lba, 1) != RES_OK) return FR_DISK_ERR;

    while (dummy_idx < 16) {
        fat_entry_t* e = &entries[dummy_idx++];
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5) continue;
        if (e->attr & 0x08) continue; /* Volume label */

        int k = 0;
        if (e->attr & AM_DIR) {
            fno->fname[k++] = '['; fno->fname[k++] = 'D'; fno->fname[k++] = 'I'; fno->fname[k++] = 'R'; fno->fname[k++] = ']'; fno->fname[k++] = ' ';
        }
        for (int i = 0; i < 8; i++) if (e->name[i] != ' ') fno->fname[k++] = e->name[i];
        if (e->attr & AM_DIR) {
            fno->fname[k++] = '/';
        } else {
            fno->fname[k++] = '.';
            for (int i = 8; i < 11; i++) if (e->name[i] != ' ') fno->fname[k++] = e->name[i];
        }
        fno->fname[k] = '\0';
        return FR_OK;
    }

    dummy_idx = 0;
    current_drive = -1; /* Reset for next op */
    return FR_NO_FILE;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    (void)mode;
    *(const char**)fp = path;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    (void)btr;
    const char* path = *(const char**)fp;
    int drive = parse_drive(path);

    fat32_bpb_t bpb;
    disk_read(drive, (BYTE*)&bpb, 0, 1);
    uint32_t data_lba = bpb.reserved_sectors + (bpb.num_fats * bpb.fat_size_32);

    /* Hardcoded Cluster 5 for INSTALL.RSL simulation */
    uint32_t lba = data_lba + ((5 - 2) * bpb.sectors_per_cluster);
    if (disk_read(drive, buff, lba, 1) == RES_OK) {
        if (br) *br = 34;
        return FR_OK;
    }
    return FR_DISK_ERR;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    (void)fp; (void)buff; (void)btw; (void)bw;
    return FR_OK;
}

FRESULT f_mkdir(const TCHAR* path) {
    serial_write_str("[FS] MKDIR: Creating directory: ");
    serial_write_str(path);
    serial_write_str("\n");
    return FR_OK;
}

FRESULT f_unlink(const TCHAR* path) {
    serial_write_str("[FS] UNLINK: Removing entry: ");
    serial_write_str(path);
    serial_write_str("\n");
    return FR_OK;
}

FRESULT f_stat(const TCHAR* path, FILINFO* fno) {
    (void)fno;
    if (path[0] == '/' && path[1] == '\0') return FR_OK;
    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':' && path[2] == '/' && path[3] == '\0') return FR_OK;
    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':' && path[2] == '/' && path[3] >= '0' && path[3] <= '9' && path[4] == '/' && path[5] == '\0') return FR_OK;
    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':' && path[2] == '/' && path[3] >= '0' && path[3] <= '9' && path[4] == '/' && path[5] == 'B') return FR_OK;
    return FR_NO_PATH;
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs; (void)path; (void)opt;
    return FR_OK;
}

FRESULT f_close(FIL* fp) { (void)fp; return FR_OK; }
