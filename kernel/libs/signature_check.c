#include <stdint.h>
#include <stddef.h>

int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
void serial_write_str(const char* s);

void vga_print(const char* fmt, ...);

#define SOVEREIGN_SIGNATURE 0xEFBEADDE

int is_sovereign_disk(int disk_id) {
    uint32_t buffer[128]; /* 512 bytes */
    if (vdisk_read(disk_id, 0, 1, buffer) != 0) {
        return 0;
    }

    if (buffer[0] == SOVEREIGN_SIGNATURE) {
        vga_print("[VDISK] OSx2 Limemade signature 0xEFBEADDE found!\n");
        return 1;
    }

    vga_print("[VDISK] OSx2 Limemade signature mismatch (Found: 0x%x).\n", buffer[0]);
    return 0;
}
