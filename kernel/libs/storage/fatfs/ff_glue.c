#include "ff.h"
#include <kernel/libs/storage/vdisk.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv >= get_hw_disk_count()) return 1;
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    if (pdrv >= get_hw_disk_count()) {
        vga_print("[GLUE] FATAL: Invalid Physical Drive Access Request.\n");
        return RES_PARERR;
    }
    if (vdisk_read_hw(pdrv, sector, count, buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    if (pdrv >= get_hw_disk_count()) {
        vga_print("[GLUE] FATAL: Invalid Physical Drive Access Request.\n");
        return RES_PARERR;
    }
    if (vdisk_write_hw(pdrv, sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DWORD get_fattime(void) {
    return 0;
}
