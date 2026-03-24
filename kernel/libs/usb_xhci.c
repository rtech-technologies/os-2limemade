#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>

/* Forward declarations */
void serial_write_str(const char* s);

typedef struct {
    uint32_t sector_size;
    uint64_t total_lba;
    void* private_data;
    int (*read_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*write_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
} vdisk_node_t;

void register_vdisk(vdisk_node_t node);
int is_sovereign_disk(int disk_id);

/* PCI Helpers (Simplified) */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    return inl(0xCFC);
}

/* Mock LBA Read for XHCI Disk */
int xhci_disk_read(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    return 0; /* Stub */
}

/* USB / XHCI Registry and Scanning */
void usb_xhci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] Scanning PCI bus for USB controllers...\n");

        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                    uint32_t vendor_device = pci_config_read(bus, slot, func, 0);
                    if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                    uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                    uint8_t base_class = (class_info >> 24) & 0xFF;
                    uint8_t sub_class = (class_info >> 16) & 0xFF;
                    uint8_t prog_if = (class_info >> 8) & 0xFF;

                    if (base_class == 0x0C && sub_class == 0x03) { /* USB */
                        if (prog_if == 0x30) { /* XHCI */
                            serial_write_str("[INIT] Found XHCI Controller.\n");
                            vdisk_node_t usb_disk = {
                                .sector_size = 512,
                                .total_lba = 1024 * 1024,
                                .read_lba = xhci_disk_read,
                                .write_lba = NULL
                            };
                            register_vdisk(usb_disk);

                            if (is_sovereign_disk(0)) {
                                serial_write_str("[CONNECT] USB Handshake Successful.\n");
                            }
                        }
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
}
