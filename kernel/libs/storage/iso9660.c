#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>

void vga_print(const char* fmt, ...);
void* bump_alloc(size_t size);

int rtech_iso_init(int drive) {
    uint8_t* sector_buffer = bump_alloc(2048);
    if (!sector_buffer) return -1;

    /* Read Sector 16 (The Primary Volume Descriptor) */
    if (vdisk_read_hw(drive, 16, 1, sector_buffer) != 0) {
        return -1;
    }

    /* Verify Magic String: CD001 */
    if (sector_buffer[1] == 'C' && sector_buffer[2] == 'D' &&
        sector_buffer[3] == '0' && sector_buffer[4] == '0' &&
        sector_buffer[5] == '1') {

        vga_print("[ISO] ISO 9660 Filesystem Detected on Drive %d.\n", drive);

        /* Root Directory LBA at offset 158 (Little Endian) */
        uint32_t root_lba = *(uint32_t*)&sector_buffer[158];
        vga_print("[ISO] Root Directory LBA: %d\n", root_lba);

        return 0;
    }

    return -1;
}
