#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <stdint.h>

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    if (vdisk_read_hw((int)pdrv, (uint64_t)sector, count, buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    if (vdisk_write_hw((int)pdrv, (uint64_t)sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DSTATUS disk_status(BYTE pdrv) {
    (void)pdrv;
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
    (void)pdrv;
    return 0;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    (void)pdrv; (void)cmd; (void)buff;
    return RES_OK;
}

DWORD get_fattime(void) {
    return 0;
}
