#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

int vdisk_read_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
int vdisk_write_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
bool vdisk_is_atapi(int hw_id);
void serial_write_str(const char* s);

int get_hw_disk_count(void);

DSTATUS disk_status(BYTE pdrv) {
    if ((int)pdrv >= get_hw_disk_count()) return 1; /* STA_NOINIT */
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if ((int)pdrv >= get_hw_disk_count()) return 1; /* STA_NOINIT */
    return 0;
}

void forensic_panic(const char* message, void* state);

void* bump_alloc(size_t size);
DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, uint32_t count) {
    /* Hardware Guard: Verify drive index exists */
    if ((int)pdrv >= get_hw_disk_count()) {
        serial_write_str("[GLUE] FATAL: Invalid Physical Drive Access Request.\n");
        forensic_panic("DISK READ OUT OF BOUNDS", NULL);
        return RES_ERROR;
    }

    /* ATAPI Translation: 1 Physical Block (2048) = 4 Logical Sectors (512) */
    if (vdisk_is_atapi((int)pdrv)) {
        uint8_t* temp_block = bump_alloc(2048);
        if (!temp_block) return RES_ERROR;
        for (uint32_t i = 0; i < count; i++) {
            uint64_t logical_sector = (uint64_t)sector + i;
            uint64_t physical_lba = logical_sector / 4;
            uint32_t offset = (logical_sector % 4) * 512;

            if (vdisk_read_hw((int)pdrv, physical_lba, 1, temp_block) != 0) return RES_ERROR;

            uint8_t* dst = &buff[i * 512];
            for (int j = 0; j < 512; j++) dst[j] = temp_block[offset + j];
        }
        return RES_OK;
    }

    if (vdisk_read_hw((int)pdrv, (uint64_t)sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, uint32_t count) {
    /* Hardware Guard: Verify drive index exists */
    if ((int)pdrv >= get_hw_disk_count()) {
        serial_write_str("[GLUE] FATAL: Invalid Physical Drive Access Request.\n");
        forensic_panic("DISK WRITE OUT OF BOUNDS", NULL);
        return RES_ERROR;
    }

    if (vdisk_write_hw((int)pdrv, (uint64_t)sector, count, (void*)buff) == 0) return RES_OK;
    return RES_ERROR;
}
