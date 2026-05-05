#include <kernel/libs/core/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <kernel/libs/storage/cherryusb/usb_osal.h>

void serial_write_str(const char* s);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
uint64_t get_hhdm_offset(void);
void pit_wait_ms(uint32_t ms);

static void* xhci_base = NULL;

void* get_xhci_base(void) {
    return xhci_base;
}

void xhci_bios_handover(uint8_t bus, uint8_t slot, uint8_t func, void* base) {
    (void)bus; (void)slot; (void)func;
    uint32_t hccparams1 = *(volatile uint32_t*)((uint8_t*)base + 0x10);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;

    if (xecp == 0) return;

    volatile uint32_t* ext_cap = (volatile uint32_t*)((uint8_t*)base + (xecp << 2));

    while (ext_cap) {
        uint32_t cap_id = *ext_cap & 0xFF;
        if (cap_id == 1) { /* USB Legacy Support */
            // Quartermaster Fix: [XHCI BIOS/OS Handover]
            serial_write_str("[XHCI] USB Legacy Support found. Requesting Handover...\n");
            *ext_cap |= (1 << 24); /* OS Owned Semaphore */

            int timeout = 1000;
            while ((*ext_cap & (1 << 16)) && timeout--) { /* BIOS Owned Semaphore */
                /* Wait for BIOS to release */
                pit_wait_ms(1);
            }

            if (timeout <= 0) {
                serial_write_str("[XHCI] Handover TIMEOUT. Forcing Control.\n");
                *ext_cap &= ~(1 << 16);
            } else {
                serial_write_str("[SNAP] XHCI BIOS/OS HANDOVER COMPLETE\n");
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

static void xhci_probe(uint8_t bus, uint8_t slot, uint8_t func, pci_id_t id) {
    if (xhci_base != NULL) return;
    (void)id;
    serial_write_str("[INIT] Found XHCI Controller.\n");
    pci_enable_master(bus, slot, func);

    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    uint64_t hhdm = get_hhdm_offset();
    xhci_base = (void*)(hhdm + (uint64_t)(bar0 & 0xFFFFFFF0));

    xhci_bios_handover(bus, slot, func, xhci_base);

    /* xHCI Initialization sequence */
    volatile uint32_t* usbcmd = (uint32_t*)((uint8_t*)xhci_base + *(uint8_t*)xhci_base + 0x00);
    volatile uint32_t* usbsts = (uint32_t*)((uint8_t*)xhci_base + *(uint8_t*)xhci_base + 0x04);

    /* 1. Stop Controller */
    *usbcmd &= ~(1 << 0);
    while (!(*usbsts & (1 << 0))) pit_wait_ms(1);

    /* 2. Reset Controller */
    *usbcmd |= (1 << 1);
    while (*usbcmd & (1 << 1)) pit_wait_ms(1);
    while (*usbsts & (1 << 11)) pit_wait_ms(1); /* Wait for CNR (Controller Not Ready) to clear */

    serial_write_str("[SNAP] xHCI Controller Reset Complete.\n");
}

static pci_id_t xhci_ids[] = {
    {0xFFFF, 0xFFFF, 0x0C, 0x03, 0x30}
};

void usb_xhci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        pci_driver_t driver = {
            .name = "xHCI",
            .id_table = xhci_ids,
            .id_count = 1,
            .probe = xhci_probe
        };
        pci_register_driver(driver);
    }
}
