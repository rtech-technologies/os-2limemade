#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);

DSTATUS disk_status(BYTE pdrv) { (void)pdrv; return 0; }
DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; return 0; }

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    (void)pdrv;
    /* The One Truth: 1:1 Physical Mapping. No hardcoded offsets here. */
    if (ahci_read_sectors(NULL, (uint64_t)sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    (void)pdrv;
    /* The One Truth: 1:1 Physical Mapping. FileSystem handles the offsets. */
    if (ahci_write_sectors(NULL, (uint64_t)sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}
