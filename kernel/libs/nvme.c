#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

typedef struct {
    uint32_t sector_size;
    uint64_t total_lba;
    void* private_data;
    int (*read_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*write_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
} vdisk_node_t;

void register_vdisk(vdisk_node_t node);

int nvme_disk_read(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    (void)priv; (void)lba; (void)count; (void)buffer;
    return 0; /* Stub */
}

void nvme_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] Scanning PCI for NVMe controllers...\n");
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                uint32_t vendor_device = pci_config_read(bus, slot, 0, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                uint32_t class_info = pci_config_read(bus, slot, 0, 0x08);
                uint8_t base_class = (class_info >> 24) & 0xFF;
                uint8_t sub_class = (class_info >> 16) & 0xFF;

                if (base_class == 0x01 && sub_class == 0x08) { /* Mass Storage, NVMe */
                    serial_write_str("[INIT] Found NVMe Controller.\n");
                    vdisk_node_t nvme_disk = {
                        .sector_size = 512,
                        .total_lba = 1024 * 1024 * 100,
                        .read_lba = nvme_disk_read,
                        .write_lba = NULL
                    };
                    register_vdisk(nvme_disk);
                }
            }
        }
    }
}
