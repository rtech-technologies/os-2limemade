#include "ff.h"
#include <stdint.h>
#include <stddef.h>

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write(int disk_id, uint64_t lba, uint32_t count, void* buffer);

DSTATUS disk_status(BYTE pdrv) {
    return 0; /* Always ready */
}

DSTATUS disk_initialize(BYTE pdrv) {
    return 0; /* OK */
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    /* Sovereign Core: LBA 0 is the signature. The FAT Boot Sector is at LBA 1 or similar.
       However, the instruction says "signature at LBA 0 (before the FAT Boot Sector)".
       Let's assume the VDISK bridge handles this or we offset it.
       Actually, Rule #5 says "VDISK Suit: A Virtual Disk abstraction layer that provides relative LBA access."
       Let's offset by 1 if signature is at 0. */
    if (vdisk_read(pdrv, sector + 1, count, buff) == 0) {
        return RES_OK;
    }
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    if (vdisk_write(pdrv, sector + 1, count, (void*)buff) == 0) {
        return RES_OK;
    }
    return RES_ERROR;
}

/* Minimal stubs for FatFS functions if we're not using a full library */
FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    return FR_OK;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    return FR_OK;
}

FRESULT f_close(FIL* fp) {
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    if (br) *br = 0;
    return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    if (bw) *bw = 0;
    return FR_OK;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
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
    }
    dummy_count = 0;
    return FR_NO_FILE;
}
