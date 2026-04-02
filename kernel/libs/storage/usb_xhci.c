#include <kernel/libs/core/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>

void serial_write_str(const char* s);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
uint64_t get_hhdm_offset(void);

static void* xhci_base = NULL;

void* get_xhci_base(void) {
    return xhci_base;
}

void xhci_bios_handover(uint8_t bus, uint8_t slot, uint8_t func, void* base) {
    uint64_t hhdm = get_hhdm_offset();
    uint32_t cap_length = *(volatile uint8_t*)base;
    uint32_t hccparams1 = *(volatile uint32_t*)((uint8_t*)base + 0x10);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;

    if (xecp == 0) return;

    volatile uint32_t* ext_cap = (volatile uint32_t*)((uint8_t*)base + (xecp << 2));

    while (ext_cap) {
        uint32_t cap_id = *ext_cap & 0xFF;
        if (cap_id == 1) { /* USB Legacy Support */
            serial_write_str("[XHCI] USB Legacy Support found. Requesting Handover...\n");
            *ext_cap |= (1 << 24); /* OS Owned Semaphore */

            int timeout = 1000;
            while ((*ext_cap & (1 << 16)) && timeout--) { /* BIOS Owned Semaphore */
                /* Wait for BIOS to release */
                for(volatile int i=0; i<10000; i++);
            }

            if (timeout <= 0) {
                serial_write_str("[XHCI] Handover TIMEOUT. Forcing Control.\n");
                *ext_cap &= ~(1 << 16);
            } else {
                serial_write_str("[XHCI] Handover Successful.\n");
            }

            /* Disable Legacy SMIs to ensure exclusive OS ownership */
            volatile uint32_t* legsup_ctl = ext_cap + 1;
            *legsup_ctl &= 0x1F00FFFF; /* Mask out SMI enable bits */
            break;
        }

        uint32_t next = (*ext_cap >> 8) & 0xFF;
        if (next == 0) break;
        ext_cap += next;
    }
}

void usb_xhci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] Scanning PCI bus for XHCI controllers...\n");

        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                    uint32_t vendor_device = pci_config_read(bus, slot, func, 0);
                    if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                    uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                    uint8_t base_class = (class_info >> 24) & 0xFF;
                    uint8_t sub_class = (class_info >> 16) & 0xFF;
                    uint8_t prog_if = (class_info >> 8) & 0xFF;

                    if (base_class == 0x0C && sub_class == 0x03 && prog_if == 0x30) {
                        serial_write_str("[INIT] Found XHCI Controller.\n");
                        pci_enable_master(bus, slot, func);

                        uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                        uint64_t hhdm = get_hhdm_offset();
                        xhci_base = (void*)(hhdm + (uint64_t)(bar0 & 0xFFFFFFF0));

                        xhci_bios_handover(bus, slot, func, xhci_base);
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
}
