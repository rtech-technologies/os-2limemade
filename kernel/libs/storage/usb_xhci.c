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

static volatile uint32_t* xhci_find_cap(void* base, uint8_t id) { uint32_t xecp = (*(volatile uint32_t*)((uint8_t*)base + 0x10) >> 16) & 0xFFFF; if (!xecp) return NULL; volatile uint32_t* cap = (volatile uint32_t*)((uint8_t*)base + (xecp << 2)); while (cap) { if ((*cap & 0xFF) == id) return cap; uint8_t next = (*cap >> 8) & 0xFF; if (!next) break; cap += (next << 0); } return NULL; }

void* get_xhci_base(void) {
    return xhci_base;
}

void pit_wait_ms(uint32_t ms);
void xhci_bios_handover(void* base) { volatile uint32_t* cap = xhci_find_cap(base, 1); if (cap) { *cap |= (1 << 24); int timeout = 1000; while ((*cap & (1 << 16)) && timeout--) pit_wait_ms(1); if (timeout <= 0) *cap &= ~(1 << 16); *(cap + 1) &= 0x1F00FFFF; } }

typedef struct { void* reg_base; uint8_t bus; uint8_t slot; uint8_t func; } cherry_xhci_hal_t;
void usbh_xhci_init(cherry_xhci_hal_t* hal);

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

                        xhci_bios_handover(xhci_base);
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
}
