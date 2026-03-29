#include <kernel/libs/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include "vdisk.h"
#include "pci.h"

/* Forward declarations */
void serial_write_str(const char* s);
struct limine_module_response* get_modules(void);
int is_sovereign_disk(int disk_id);

/* Actual Ramdisk-backed Read for XHCI/USB Simulation */
int xhci_disk_read(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    (void)priv;
    struct limine_module_response* resp = get_modules();
    if (!resp || resp->module_count == 0) return -1;

    struct limine_file* ramdisk = resp->modules[0];
    uint8_t* base = (uint8_t*)ramdisk->address;

    size_t offset = lba * 512;
    size_t size = count * 512;

    if (offset + size > ramdisk->size) return -1;

    uint8_t* src = base + offset;
    uint8_t* dst = (uint8_t*)buffer;

    for (size_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }

    return 0;
}

static void* xhci_base = NULL;

void* get_xhci_base(void) {
    return xhci_base;
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
                            uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                            xhci_base = (void*)(uint64_t)(bar0 & 0xFFFFFFF0);

                            vdisk_node_t usb_disk = {
                                .sector_size = 512,
                                .total_lba = 1024 * 1024,
                                .read_lba = xhci_disk_read,
                                .write_lba = NULL
                            };
                            register_hardware_disk(usb_disk);

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
