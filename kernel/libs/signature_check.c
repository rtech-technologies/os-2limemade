#include <stdint.h>
#include <stddef.h>

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
void serial_write_str(const char* s);

#define SOVEREIGN_SIGNATURE 0xDEADBEEF

int is_sovereign_disk(int disk_id) {
    uint32_t buffer[128]; /* 512 bytes */
    if (vdisk_read(disk_id, 0, 1, buffer) != 0) {
        return 0;
    }

    if (buffer[0] == SOVEREIGN_SIGNATURE) {
        serial_write_str("[VDISK] Sovereign signature 0xDEADBEEF found!\n");
        return 1;
    }

    serial_write_str("[VDISK] Sovereign signature mismatch.\n");
    return 0;
}
