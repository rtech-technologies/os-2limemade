#include <kernel/libs/core/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>

void serial_write_str(const char* s);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
uint64_t get_hhdm_offset(void);
void pit_wait_ms(uint32_t ms);
void usb_audit_log(const char* event, const char* details);

static void* xhci_base = NULL;

void* get_xhci_base(void) {
    return xhci_base;
}

void xhci_bios_handover(void* base) {
    uint32_t hccparams1 = *(volatile uint32_t*)((uint8_t*)base + 0x10);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;

    if (xecp == 0) return;

    volatile uint32_t* ext_cap = (volatile uint32_t*)((uint8_t*)base + (xecp << 2));

    while (1) {
        uint32_t cap_id = *ext_cap & 0xFF;
        if (cap_id == 1) { /* USB Legacy Support */
            usb_audit_log("XHCI-HANDOVER", "Requesting Legacy Support Handover");
            *ext_cap |= (1 << 24); /* OS Owned Semaphore */

            int timeout = 1000;
            while ((*ext_cap & (1 << 16)) && timeout--) { /* BIOS Owned Semaphore */
                pit_wait_ms(1);
            }

            if (timeout <= 0) {
                usb_audit_log("XHCI-HANDOVER", "TIMEOUT - Forcing Control (Caution: USB Keyboard may drop)");
                *ext_cap &= ~(1 << 16);
            } else {
                usb_audit_log("XHCI-HANDOVER", "SUCCESS - BIOS Released Controller");
            }

            /* 🧱 Blocky Fix: Preserve BIOS Keyboard Emulation
               We disable the SMI-based traps that might cause hangs,
               but we leave the LegSup Control/Status register in a state
               where PS/2 emulation can potentially survive if the BIOS allows. */
            volatile uint32_t* legsup_ctl = (volatile uint32_t*)ext_cap + 1;
            *legsup_ctl = (*legsup_ctl & 0x1F00FFFF) | 0x80000000; /* Enable USB Keyboard/Mouse SMI, but carefully */
            break;
        }

        uint32_t next = (*ext_cap >> 8) & 0xFF;
        if (next == 0) break;
        ext_cap += next;
    }
}

void xhci_init(uint8_t bus, uint8_t slot, uint8_t func) {
    usb_audit_log("XHCI-INIT", "Found Controller");
    pci_enable_master(bus, slot, func);

    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
    uint64_t phys_base = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);
    xhci_base = (void*)(get_hhdm_offset() + phys_base);

    xhci_bios_handover(xhci_base);

    uint32_t cap_len = *(volatile uint8_t*)xhci_base;
    volatile uint32_t* usbcmd = (volatile uint32_t*)((uint8_t*)xhci_base + cap_len);
    *usbcmd |= (1 << 1); /* HCRST */
    int timeout = 1000;
    while ((*usbcmd & (1 << 1)) && timeout--) pit_wait_ms(1);

    usb_audit_log("XHCI-RESET", "Controller Reset Complete");
}

void usb_xhci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        pci_driver_t xhci_driver = {
            .vendor_id = 0xFFFF, .device_id = 0xFFFF,
            .class_code = 0x0C, .subclass_code = 0x03, .prog_if = 0x30,
            .init = xhci_init
        };
        pci_register_driver(xhci_driver);
    }
}
