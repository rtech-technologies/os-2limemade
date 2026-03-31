#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

int vdisk_read_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);

DSTATUS disk_status(BYTE pdrv) { (void)pdrv; return 0; }
DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; return 0; }

int get_hw_disk_count(void);

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    /* Hardware Guard: Verify drive index exists */
    if ((int)pdrv >= get_hw_disk_count()) {
        serial_write_str("[GLUE] Error: Invalid Physical Drive Access.\n");
        return RES_ERROR;
    }

    if (vdisk_read_hw((int)pdrv, (uint64_t)sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    /* Hardware Guard: Verify drive index exists */
    if ((int)pdrv >= get_hw_disk_count()) {
        serial_write_str("[GLUE] Error: Invalid Physical Drive Access.\n");
        return RES_ERROR;
    }

    if (vdisk_write_hw((int)pdrv, (uint64_t)sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}
