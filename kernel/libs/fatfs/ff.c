#include "ff.h"
#include <include/rsl.h>
#include <kernel/libs/vdisk.h>
#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void serial_write_str(const char* s);
void serial_print_hex(const char* label, uint16_t val);

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
    if (disk_read(drive, mbr, 0, 1) != RES_OK) return FR_DISK_ERR;
    uint8_t* p = &mbr[446];
    p[0] = 0x80; p[4] = 0x0C;
    *(uint32_t*)&p[8] = 2048;
    *(uint32_t*)&p[12] = 129024;
    mbr[510] = 0x55; mbr[511] = 0xAA;
    if (disk_write(drive, mbr, 0, 1) != RES_OK) return FR_DISK_ERR;
    return FR_OK;
}

void pit_wait_ms(uint32_t ms);

FRESULT f_mount(FATFS* fs, int drive) {
    uint8_t sector[2048];
    uint64_t part_lba = vdisk_get_offset(drive);

    if (part_lba == 0) {
        /* No registry offset found, attempt partition discovery */
        if (disk_read(drive, sector, 1, 1) == RES_OK && *(uint64_t*)sector == 0x5452415020494645ULL) {
            /* GPT Header Found */
            if (disk_read(drive, sector, 2, 1) == RES_OK) {
                part_lba = *(uint64_t*)&sector[32];
            }
        }
        if (part_lba == 0) {
            /* Fallback to MBR check */
            if (disk_read(drive, sector, 0, 1) == RES_OK) {
                for (int i = 0; i < 4; i++) {
                    uint8_t* p = &sector[446 + (i * 16)];
                    if (p[4] == 0x0C) { part_lba = *(uint32_t*)&p[8]; break; }
                }
            }
        }
    }

    if (part_lba == 0) return FR_NO_FILESYSTEM;
    if (disk_read(drive, sector, part_lba, 1) != RES_OK) return FR_DISK_ERR;
    if (sector[510] != 0x55 || sector[511] != 0xAA) return FR_NO_FILESYSTEM;

    fs->partition_lba = part_lba;
    fs->reserved_sectors = *(uint16_t*)&sector[14];
    fs->n_fats = sector[16];
    fs->sectors_per_fat = *(uint32_t*)&sector[36];
    fs->sectors_per_cluster = sector[13];
    fs->root_cluster = *(uint32_t*)&sector[44];
    fs->sector_size = *(uint16_t*)&sector[11];
    fs->data_lba = (LBA_t)part_lba + (LBA_t)fs->reserved_sectors + ((LBA_t)fs->n_fats * (LBA_t)fs->sectors_per_fat);
    fs->drv = (uint8_t)drive;
    fs->active = true;
    fs->ro = false;
    return FR_OK;
}

static uint32_t get_next_cluster(FATFS* fs, uint32_t cluster) {
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint64_t fat_sector = fs->partition_lba + (uint64_t)fs->reserved_sectors + ((uint64_t)cluster * 4 / ss);
    uint32_t fat_offset = (cluster * 4) % ss;

    /* Buffer Isolation: Remove static to prevent corruption */
    uint8_t sector_buf[2048];
    if (disk_read(fs->drv, sector_buf, fat_sector, 1) != RES_OK) return 0x0FFFFFFF;

    return (*(uint32_t*)&sector_buf[fat_offset]) & 0x0FFFFFFF;
}
uint32_t f_get_next_cluster(FATFS* fs, uint32_t cluster) { return get_next_cluster(fs, cluster); }

static void to_sfn(const char* src, uint8_t* dst) {
    /* Sanitize the SFN: Explicitly clear with 0x20 (spaces) */
    for (int i = 0; i < 11; i++) dst[i] = 0x20;
    int i = 0, j = 0;
    while (src[i] && src[i] != '.' && src[i] != '/' && j < 8) {
        dst[j++] = src[i++];
    }
    while (src[i] && src[i] != '.' && src[i] != '/') i++;
    if (src[i] == '.') {
        i++; j = 8;
        while (src[i] && src[i] != '/' && j < 11) {
            dst[j++] = src[i++];
        }
    }
    for (int k = 0; k < 11; k++) if (dst[k] >= 'a' && dst[k] <= 'z') dst[k] -= 32;
}

static uint64_t get_sector_lba(FATFS* fs, uint32_t cluster) {
    return fs->data_lba + (uint64_t)(cluster - 2) * fs->sectors_per_cluster;
}

static uint32_t find_entry(FATFS* fs, uint32_t dir_cluster, const char* name, fat_dir_entry_t* out_entry, uint64_t* out_lba, uint32_t* out_idx) {
    if (!fs->active) return 0;
    uint32_t cluster = dir_cluster;
    uint8_t sfn[11];
    to_sfn(name, sfn);
    while (cluster < 0x0FFFFFF8) {
        uint64_t lba = get_sector_lba(fs, cluster);
        uint8_t entries_buf[2048]; /* Safety buffer for ATAPI/CD-ROM sectors */
        fat_dir_entry_t* entries = (fat_dir_entry_t*)entries_buf;
        for (uint8_t s = 0; s < fs->sectors_per_cluster; s++) {
            if (disk_read(fs->drv, (BYTE*)entries, lba + s, 1) != RES_OK) return 0;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0) return 0;
                if (entries[i].name[0] == 0xE5) continue;
                bool match = true;
                for (int k = 0; k < 11; k++) if (entries[i].name[k] != sfn[k]) match = false;
                if (match) {
                    if (out_entry) *out_entry = entries[i];
                    if (out_lba) *out_lba = lba + s;
                    if (out_idx) *out_idx = i;
                    return (uint32_t)entries[i].first_cluster_low | ((uint32_t)entries[i].first_cluster_high << 16);
                }
            }
        }
        cluster = get_next_cluster(fs, cluster);
    }
    return 0;
}

static uint32_t resolve_path_to_cluster(FATFS* fs, const char* path, fat_dir_entry_t* out_entry, uint64_t* out_lba, uint32_t* out_idx) {
    uint32_t cluster = fs->root_cluster;
    int i = 0;

    if (out_lba) *out_lba = 0;
    if (out_idx) *out_idx = 0;

    /* Drain leading slashes */
    while (path[i] == '/') i++;

    /* Root or current relative dir case */
    if (path[i] == '\0' || (path[i] == '.' && path[i+1] == '\0')) {
        if (out_entry) {
            for(int k=0; k<11; k++) out_entry->name[k] = ' ';
            out_entry->attr = AM_DIR;
            out_entry->size = 0;
        }
        return cluster;
    }

    while (path[i]) {
        char name[256];
        int j = 0;
        while (path[i] && path[i] != '/') {
            if (j < 255) name[j++] = path[i];
            i++;
        }
        name[j] = '\0';

        /* Drain redundant slashes - MUST advance i to avoid infinite loop */
        while (path[i] == '/') i++;

        uint32_t next = find_entry(fs, cluster, name, out_entry, out_lba, out_idx);
        if (!next) return 0;
        if (path[i] == '\0') return next;
        cluster = next;
    }
    return 0;
}

static uint32_t find_free_cluster(FATFS* fs);
static FRESULT set_cluster_link(FATFS* fs, uint32_t cluster, uint32_t next);

FRESULT f_open(FATFS* fs, FIL* fp, const TCHAR* path, BYTE mode) {
    if (!fs || !fs->active) return FR_NOT_ENABLED;
    fp->obj = fs;
    fat_dir_entry_t entry;
    uint64_t entry_lba;
    uint32_t entry_idx;
    uint32_t cluster = resolve_path_to_cluster(fs, path, &entry, &entry_lba, &entry_idx);
    if (!cluster) {
        if (mode & (FA_CREATE_ALWAYS | FA_CREATE_NEW | FA_OPEN_ALWAYS)) {
            char dir_path[256]; int last_slash = -1;
            for(int k=0; path[k]; k++) if(path[k] == '/') last_slash = k;
            uint32_t parent_cluster;
            const char* filename;
            if (last_slash == -1) { parent_cluster = fs->root_cluster; filename = path; }
            else {
                int k;
                for(k=0; k<last_slash; k++) {
                    dir_path[k] = path[k];
                }
                dir_path[k] = '\0';
                parent_cluster = resolve_path_to_cluster(fs, dir_path, NULL, NULL, NULL);
                filename = &path[last_slash+1];
            }
            if (!parent_cluster) return FR_NO_PATH;
            uint32_t new_cluster = find_free_cluster(fs);
            if (!new_cluster) return FR_DENIED;
            set_cluster_link(fs, new_cluster, 0x0FFFFFFF);
            fat_dir_entry_t new_entry = {0};
            to_sfn(filename, new_entry.name);
            new_entry.attr = AM_ARC;
            new_entry.first_cluster_low = new_cluster & 0xFFFF;
            new_entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
            new_entry.size = 0;
            uint8_t dir_buf[2048];
            uint64_t p_lba = get_sector_lba(fs, parent_cluster);
            disk_read(fs->drv, dir_buf, p_lba, 1);
            fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
            for(int k=0; k<16; k++) {
                if (entries[k].name[0] == 0 || entries[k].name[0] == 0xE5) {
                    entries[k] = new_entry;
                    disk_write(fs->drv, dir_buf, p_lba, 1);
                    fp->sclust = new_cluster; fp->clust = new_cluster; fp->fptr = 0; fp->fsize = 0;
                    fp->entry_lba = p_lba; fp->entry_idx = k;
                    return FR_OK;
                }
            }
        }
        return FR_NO_FILE;
    }
    fp->sclust = cluster; fp->clust = fp->sclust; fp->fptr = 0; fp->fsize = entry.size;
    fp->entry_lba = entry_lba; fp->entry_idx = entry_idx;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    FATFS* fs = fp->obj;
    if (!fs || !fs->active) return FR_DENIED;
    if (fp->fptr >= fp->fsize) return FR_OK;

    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint32_t cluster_size = ss * fs->sectors_per_cluster;
    uint32_t bytes_left_in_file = fp->fsize - fp->fptr;
    if (btr > bytes_left_in_file) btr = bytes_left_in_file;

    uint32_t total_read = 0;
    uint8_t* p = (uint8_t*)buff;

    while (btr > 0) {
        uint32_t sector_in_cluster = (fp->fptr / ss) % fs->sectors_per_cluster;
        uint32_t offset_in_sector = fp->fptr % ss;
        uint64_t lba = get_sector_lba(fs, fp->clust) + sector_in_cluster;

        uint32_t can_read = ss - offset_in_sector;
        if (can_read > btr) can_read = btr;

        if (offset_in_sector == 0 && can_read == ss) {
            if (disk_read(fs->drv, p, lba, 1) != RES_OK) break;
        } else {
            /* Buffer Isolation: Use stack instead of static to prevent corruption */
            uint8_t sector_buf[2048];
            if (disk_read(fs->drv, sector_buf, lba, 1) != RES_OK) break;
            for (uint32_t i = 0; i < can_read; i++) p[i] = sector_buf[offset_in_sector + i];
        }

        p += can_read; fp->fptr += can_read; btr -= can_read; total_read += can_read;
        if (fp->fptr % cluster_size == 0 && btr > 0) {
            fp->clust = get_next_cluster(fs, fp->clust);
            if (fp->clust >= 0x0FFFFFF8) break;
        }
    }

    if (br) *br = total_read;
    return FR_OK;
}

static FRESULT set_cluster_link(FATFS* fs, uint32_t cluster, uint32_t next) {
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint64_t fat_sector = fs->partition_lba + (uint64_t)fs->reserved_sectors + ((uint64_t)cluster * 4 / ss);
    uint8_t fat_buf[ss];
    if (disk_read(fs->drv, fat_buf, fat_sector, 1) != RES_OK) return FR_DISK_ERR;
    ((uint32_t*)fat_buf)[(cluster * 4 % ss) / 4] = next & 0x0FFFFFFF;
    if (disk_write(fs->drv, fat_buf, fat_sector, 1) != RES_OK) return FR_DISK_ERR;
    return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    FATFS* fs = fp->obj;
    if (!fs || !fs->active) return FR_DENIED;
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint32_t cluster_size = ss * fs->sectors_per_cluster;
    uint32_t bytes_left = btw;
    const uint8_t* p = (const uint8_t*)buff;

    while (bytes_left > 0) {
        uint32_t sector_in_cluster = (fp->fptr / ss) % fs->sectors_per_cluster;
        uint32_t cluster_offset = fp->fptr % cluster_size;
        if (fp->fptr > 0 && cluster_offset == 0) {
            uint32_t next = get_next_cluster(fs, fp->clust);
            if (next >= 0x0FFFFFF8) {
                next = find_free_cluster(fs);
                if (!next) return FR_DENIED;
                set_cluster_link(fs, fp->clust, next);
                set_cluster_link(fs, next, 0x0FFFFFFF);
            }
            fp->clust = next;
        }
        uint64_t lba = get_sector_lba(fs, fp->clust) + sector_in_cluster;
        if (disk_write(fs->drv, p, lba, 1) != RES_OK) break;
        p += ss; fp->fptr += ss;
        if (fp->fptr > fp->fsize) fp->fsize = fp->fptr;
        if (bytes_left > ss) bytes_left -= ss; else bytes_left = 0;
    }
    if (bw) *bw = btw - bytes_left;
    return FR_OK;
}

FRESULT f_opendir(FATFS* fs, DIR* dp, const TCHAR* path) {
    if (!fs || !fs->active) return FR_NOT_ENABLED;
    uint32_t cluster = resolve_path_to_cluster(fs, path, NULL, NULL, NULL);
    if (!cluster) return FR_NO_PATH;
    dp->obj = fs; dp->sclust = cluster; dp->clust = dp->sclust; dp->index = 0;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    FATFS* fs = dp->obj;
    if (!fs || !fs->active) return FR_DENIED;
    while (dp->clust < 0x0FFFFFF8) {
        uint64_t lba = get_sector_lba(fs, dp->clust);
        uint8_t entries_buf[2048]; /* Safety buffer for ATAPI/CD-ROM sectors */
        fat_dir_entry_t* entries = (fat_dir_entry_t*)entries_buf;
        uint32_t sector_idx = (dp->index / 16);
        if (sector_idx >= fs->sectors_per_cluster) {
            dp->index = 0; dp->clust = get_next_cluster(fs, dp->clust);
            continue;
        }
        if (disk_read(fs->drv, (BYTE*)entries, lba + sector_idx, 1) != RES_OK) return FR_DISK_ERR;
        while ((dp->index % 16) < 16) {
            fat_dir_entry_t* e = &entries[dp->index % 16];
            dp->index++;
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
    }
    return FR_NO_FILE;
}

FRESULT f_mkfs(int drive) {
    uint64_t offset = vdisk_get_offset(drive);
    if (offset == 0) offset = 2048; /* Force GPT Standard offset for sovereign volumes */

    uint8_t boot[512] = {0};
    boot[0] = 0xEB; boot[1] = 0x58; boot[2] = 0x90;
    boot[11] = 0x00; boot[12] = 0x02; boot[13] = 0x08;
    boot[14] = 0x20; boot[16] = 0x02;
    *(uint32_t*)&boot[36] = 128; *(uint32_t*)&boot[44] = 2;
    boot[510] = 0x55; boot[511] = 0xAA;
    disk_write(drive, boot, offset, 1);

    uint8_t fat[512] = {0};
    *(uint32_t*)&fat[0] = 0x0FFFFFF8; *(uint32_t*)&fat[4] = 0xFFFFFFFF; *(uint32_t*)&fat[8] = 0x0FFFFFFF;
    disk_write(drive, fat, offset + 32, 1);

    uint8_t zero[512] = {0};
    disk_write(drive, zero, offset + 32 + 256, 1);
    return FR_OK;
}

static uint32_t find_free_cluster(FATFS* fs) {
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint8_t buf[ss];
    for (uint32_t s = 0; s < fs->sectors_per_fat; s++) {
        uint64_t lba = fs->partition_lba + (uint64_t)fs->reserved_sectors + s;
        if (disk_read(fs->drv, buf, lba, 1) == RES_OK) {
            uint32_t* fat = (uint32_t*)buf;
            for (uint32_t i = 0; i < ss/4; i++) {
                if ((fat[i] & 0x0FFFFFFF) == 0) {
                    uint32_t res = (s * (ss/4)) + i;
                    if (res >= 2) return res;
                }
            }
        }
    }
    return 0;
}

FRESULT f_mkdir(FATFS* fs, const TCHAR* path) {
    if (!fs || !fs->active) return FR_DENIED;
    char dir_path[256]; int last_slash = -1;
    for(int k=0; path[k]; k++) if(path[k] == '/') last_slash = k;
    uint32_t parent_cluster;
    const char* filename;
    if (last_slash == -1) { parent_cluster = fs->root_cluster; filename = path; }
    else {
        int k;
        for(k=0; k<last_slash; k++) dir_path[k] = path[k]; dir_path[k] = '\0';
        parent_cluster = resolve_path_to_cluster(fs, dir_path, NULL, NULL, NULL);
        filename = &path[last_slash+1];
    }
    if (!parent_cluster) return FR_NO_PATH;
    if (find_entry(fs, parent_cluster, filename, NULL, NULL, NULL) != 0) return FR_EXIST;
    uint32_t new_cluster = find_free_cluster(fs);
    if (!new_cluster) return FR_DENIED;
    uint8_t zero[512] = {0};
    uint64_t lba = get_sector_lba(fs, new_cluster);
    disk_write(fs->drv, zero, lba, 1);
    set_cluster_link(fs, new_cluster, 0x0FFFFFFF);
    fat_dir_entry_t entry = {0};
    to_sfn(filename, entry.name);
    entry.attr = AM_DIR;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;

    uint8_t dir_buf[2048];
    for (uint32_t s = 0; s < fs->sectors_per_cluster; s++) {
        uint64_t p_lba = get_sector_lba(fs, parent_cluster) + s;
        disk_read(fs->drv, dir_buf, p_lba, 1);
        fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
        for(int k=0; k<16; k++) {
            if (entries[k].name[0] == 0 || entries[k].name[0] == 0xE5) {
                entries[k] = entry;
                disk_write(fs->drv, dir_buf, p_lba, 1);
                return FR_OK;
            }
        }
    }
    return FR_DENIED;
}

FRESULT f_unlink(FATFS* fs, const TCHAR* path) {
    if (!fs || !fs->active) return FR_DENIED;
    fat_dir_entry_t entry;
    uint64_t entry_lba;
    uint32_t entry_idx;
    if (!resolve_path_to_cluster(fs, path, &entry, &entry_lba, &entry_idx)) return FR_NO_FILE;
    uint8_t dir_buf[2048];
    disk_read(fs->drv, dir_buf, entry_lba, 1);
    fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
    entries[entry_idx].name[0] = 0xE5;
    disk_write(fs->drv, dir_buf, entry_lba, 1);
    return FR_OK;
}

FRESULT f_stat(FATFS* fs, const TCHAR* path, FILINFO* fno) {
    if (!fs || !fs->active) return FR_NOT_ENABLED;
    fat_dir_entry_t entry;
    if (resolve_path_to_cluster(fs, path, &entry, NULL, NULL)) {
        if (fno) { fno->fsize = entry.size; fno->fattrib = entry.attr; }
        return FR_OK;
    }
    return FR_NO_PATH;
}

FRESULT f_close(FIL* fp) {
    if (!fp || !fp->obj) return FR_INVALID_OBJECT;
    /* Protect LBA 0: Cannot sync metadata if entry_lba is 0 (MBR or Root) */
    if (fp->entry_lba == 0) return FR_INVALID_OBJECT;
    uint8_t dir_buf[2048];
    if (disk_read(fp->obj->drv, dir_buf, fp->entry_lba, 1) != RES_OK) return FR_DISK_ERR;
    fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
    entries[fp->entry_idx].size = fp->fsize;
    entries[fp->entry_idx].first_cluster_low = fp->sclust & 0xFFFF;
    entries[fp->entry_idx].first_cluster_high = (fp->sclust >> 16) & 0xFFFF;
    if (disk_write(fp->obj->drv, dir_buf, fp->entry_lba, 1) != RES_OK) return FR_DISK_ERR;
    return FR_OK;
}
