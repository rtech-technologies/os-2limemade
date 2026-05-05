#include <kernel/libs/core/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include "kernel/libs/cherryusb/common/usb_list.h"
#include "kernel/libs/cherryusb/common/usb_hc.h"

void serial_write_str(const char* s);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
uint64_t get_hhdm_offset(void);

static void* xhci_base = NULL;

void* get_xhci_base(void) {
    return xhci_base;
}

volatile uint32_t* xhci_find_cap(void* base, uint32_t target_id) {
    uint32_t hccparams1 = *(volatile uint32_t*)((uint8_t*)base + 0x10);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;
    if (xecp == 0) return NULL;
    volatile uint32_t* ext_cap = (volatile uint32_t*)((uint8_t*)base + (xecp << 2));
    while (ext_cap) {
        if ((*ext_cap & 0xFF) == target_id) return ext_cap;
        uint32_t next = (*ext_cap >> 8) & 0xFF;
        if (next == 0) break;
        ext_cap += next;
    }
    return NULL;
}

void xhci_bios_handover(void* base) {
    volatile uint32_t* legsup = xhci_find_cap(base, 1);
    if (!legsup) return;

    serial_write_str("[XHCI] USB Legacy Support found.\n");
    *legsup |= (1 << 24); /* Set OSOwned */

    int timeout = 1000;
    while ((*legsup & (1 << 16)) && timeout--) {
        for(volatile int i=0; i<10000; i++);
    }

    if (timeout <= 0) {
        serial_write_str("[XHCI] Handover Timeout. Force Control.\n");
        *legsup &= ~(1 << 16);
    }

    *(legsup + 1) &= 0x1F00FFFF; /* Disable SMIs */
}

void xhci_bind(uint8_t bus, uint8_t slot, uint8_t func) {
    serial_write_str("[INIT] Found XHCI Controller.\n");
    pci_enable_master(bus, slot, func);

    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    uint64_t hhdm = get_hhdm_offset();
    xhci_base = (void*)(hhdm + (uint64_t)(bar0 & 0xFFFFFFF0));

    xhci_bios_handover(xhci_base);

    extern int usbh_initialize(uint8_t busid, uintptr_t reg_base, void* handler);
    usbh_initialize(0, (uintptr_t)xhci_base, NULL);
}

void usb_xhci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        pci_driver_t xhci_driver = {
            .name = "xHCI Controller",
            .target = { .class_id = 0x0C, .subclass = 0x03, .prog_if = 0x30 },
            .bind = xhci_bind
        };
        pci_register_driver(xhci_driver);
        serial_write_str("[INIT] xHCI PCI driver registered.\n");
    }
}
