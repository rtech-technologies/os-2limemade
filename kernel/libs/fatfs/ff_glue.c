#include "ff.h"
#include <stdint.h>
#include <stddef.h>

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer);

DSTATUS disk_status(BYTE pdrv) {
    (void)pdrv;
    return 0; /* Always ready */
}

DSTATUS disk_initialize(BYTE pdrv) {
    (void)pdrv;
    return 0; /* OK */
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    /* OSx2 Limemade Core: Map FatFS pdrv to VDISK registry.
       Apply the +1 offset to skip the LBA 0 signature (Rule #5). */
    if (vdisk_read((int)pdrv, (uint64_t)sector + 1, count, (void*)buff) == 0) {
        return RES_OK;
    }
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    if (vdisk_write((int)pdrv, (uint64_t)sector + 1, count, (void*)buff) == 0) {
        return RES_OK;
    }
    return RES_ERROR;
}

/* Minimal stubs for FatFS functions */
FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs; (void)path; (void)opt;
    return FR_OK;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    (void)fp; (void)path; (void)mode;
    return FR_OK;
}

FRESULT f_close(FIL* fp) {
    (void)fp;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    (void)fp; (void)buff; (void)btr;
    if (br) *br = 0;
    return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    (void)fp; (void)buff; (void)btw;
    if (bw) *bw = 0;
    return FR_OK;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    (void)dp; (void)path;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    (void)dp;
    static int dummy_count = 0;
    if (dummy_count == 0) {
        for(int i=0; "bin" [i]; i++) fno->fname[i] = "bin" [i];
        fno->fname[3] = '\0';
        dummy_count++;
        return FR_OK;
    } else if (dummy_count == 1) {
        for(int i=0; "rsl.sh" [i]; i++) fno->fname[i] = "rsl.sh" [i];
        fno->fname[6] = '\0';
        dummy_count++;
        return FR_OK;
    } else if (dummy_count == 2) {
        for(int i=0; "INSTALL.rsl" [i]; i++) fno->fname[i] = "INSTALL.rsl" [i];
        fno->fname[11] = '\0';
        dummy_count++;
        return FR_OK;
    }
    dummy_count = 0;
    return FR_NO_FILE;
}
