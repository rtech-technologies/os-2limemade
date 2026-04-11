#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <include/xhci.h>
#include <include/mouse.h>
#include <include/config.h>

void serial_write_str(const char* s);
void serial_print(const char* fmt, ...);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
uint64_t get_hhdm_offset(void);
uint64_t vmm_get_phys(void* virt);

static void* xhci_base = NULL;
static uint32_t xhci_cap_len = 0;
static uint32_t xhci_rt_off = 0;

static xhci_trb_t* event_ring = NULL;
static xhci_erst_entry_t* erst = NULL;
static uint64_t* dcbaap = NULL;

static int event_idx = 0;
static bool event_cycle = true;

/* Scancode Map: USB HID to ASCII (Partial) */
static char usb_map[256] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd', [0x08] = 'e', [0x09] = 'f',
    [0x0A] = 'g', [0x0B] = 'h', [0x0C] = 'i', [0x0D] = 'j', [0x0E] = 'k', [0x0F] = 'l',
    [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p', [0x14] = 'q', [0x15] = 'r',
    [0x16] = 's', [0x17] = 't', [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x',
    [0x1C] = 'y', [0x1D] = 'z', [0x1E] = '1', [0x1F] = '2', [0x20] = '3', [0x21] = '4',
    [0x22] = '5', [0x23] = '6', [0x24] = '7', [0x25] = '8', [0x26] = '9', [0x27] = '0',
    [0x28] = '\n', [0x2C] = ' ', [0x2A] = '\b', [0x2B] = '\t'
};

void xhci_rt_write(uint32_t reg, uint32_t val) {
    if (!xhci_base) return;
    *(volatile uint32_t*)((uint8_t*)xhci_base + xhci_rt_off + reg) = val;
}

uint32_t xhci_rt_read(uint32_t reg) {
    if (!xhci_base) return 0;
    return *(volatile uint32_t*)((uint8_t*)xhci_base + xhci_rt_off + reg);
}

static inline void xhci_op_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)((uint8_t*)xhci_base + xhci_cap_len + reg) = val;
}

static inline uint32_t xhci_op_read(uint32_t reg) {
    return *(volatile uint32_t*)((uint8_t*)xhci_base + xhci_cap_len + reg);
}

char usb_keyboard_poll(void) {
    if (!xhci_base || !event_ring) return 0;

    /* Check for new events in the ring */
    xhci_trb_t* ev = &event_ring[event_idx];
    if ((ev->control & 0x01) != (uint32_t)event_cycle) return 0;

    uint8_t type = (ev->control >> 10) & 0x3F;
    char result = 0;

    if (type == 32) { /* Transfer Event */
        uint8_t* report = (uint8_t*)(ev->ptr);
        if (report) {
            /* Basic HID Detection: Keyboard vs Mouse */
            if (report[1] == 0 && report[2] != 0) { /* Keyboard Report */
                uint8_t code = report[2];
                result = usb_map[code];
            } else if (report[0] & 0x07 || report[1] != 0 || report[2] != 0) { /* Mouse Report */
                mouse_state_t* ms = get_mouse_state();
                if (ms) {
                    ms->left_button = report[0] & 0x01;
                    ms->right_button = report[0] & 0x02;
                    ms->middle_button = report[0] & 0x04;
                    ms->x += (int8_t)report[1];
                    ms->y += (int8_t)report[2];
                    ms->active = true;
                }
            }
        }
    }

    /* Advance Event Ring */
    event_idx = (event_idx + 1) % 256;
    if (event_idx == 0) event_cycle = !event_cycle;

    /* Update Dequeue Pointer */
    uint64_t erdp_phys = vmm_get_phys(&event_ring[event_idx]);
    xhci_rt_write(XHCI_RT_ERDP(0), (uint32_t)erdp_phys | 0x08);
    xhci_rt_write(XHCI_RT_ERDP(0) + 4, (uint32_t)(erdp_phys >> 32));

    return result;
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
            serial_write_str("[XHCI] Requesting BIOS Handover...\n");
            *ext_cap |= (1 << 24); /* OS Owned Semaphore */
            int timeout = 1000;
            while ((*ext_cap & (1 << 16)) && timeout--) {
                for(volatile int i=0; i<10000; i++);
            }
            if (timeout <= 0) *ext_cap &= ~(1 << 16);
            volatile uint32_t* legsup_ctl = ext_cap + 1;
            *legsup_ctl &= 0x1F00FFFF;
            break;
        }
        uint32_t next = (*ext_cap >> 8) & 0xFF;
        if (next == 0) break;
        ext_cap += next;
    }
}

void usb_xhci_service(kernel_event_t event) {
#if !defined(CONFIG_INTERFACE_PS2)
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] Scanning for XHCI...\n");
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                    uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                    if (((class_info >> 24) & 0xFF) == 0x0C && ((class_info >> 16) & 0xFF) == 0x03 && ((class_info >> 8) & 0xFF) == 0x30) {
                        pci_enable_master(bus, slot, func);
                        uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                        xhci_base = (void*)(get_hhdm_offset() + (uint64_t)(bar0 & 0xFFFFFFF0));
                        xhci_bios_handover(bus, slot, func, xhci_base);

                        xhci_cap_len = *(volatile uint8_t*)xhci_base;
                        xhci_rt_off = *(volatile uint32_t*)((uint8_t*)xhci_base + XHCI_CAP_RTSOFF);

                        xhci_op_write(XHCI_OP_USBCMD, xhci_op_read(XHCI_OP_USBCMD) & ~0x01);
                        while(!(xhci_op_read(XHCI_OP_USBSTS) & 0x01));
                        xhci_op_write(XHCI_OP_USBCMD, 0x02);
                        while(xhci_op_read(XHCI_OP_USBCMD) & 0x02);

                        void* slab_alloc_aligned(int id, size_t size, size_t align);
                        xhci_trb_t* cmd_ring = slab_alloc_aligned(0, 4096, 64);
                        event_ring = slab_alloc_aligned(0, 4096, 64);
                        erst = slab_alloc_aligned(0, 4096, 64);
                        dcbaap = slab_alloc_aligned(0, 4096, 64);

                        xhci_op_write(XHCI_OP_CONFIG, *(volatile uint32_t*)((uint8_t*)xhci_base + 0x04) & 0xFF);
                        xhci_op_write(XHCI_OP_DCBAAP, (uint32_t)vmm_get_phys(dcbaap));
                        xhci_op_write(XHCI_OP_DCBAAP + 4, (uint32_t)(vmm_get_phys(dcbaap) >> 32));
                        xhci_op_write(XHCI_OP_CRCR, (uint32_t)vmm_get_phys(cmd_ring) | 1);
                        xhci_op_write(XHCI_OP_CRCR + 4, (uint32_t)(vmm_get_phys(cmd_ring) >> 32));

                        erst[0].ptr = vmm_get_phys(event_ring);
                        erst[0].size = 256;
                        xhci_rt_write(XHCI_RT_ERSTSZ(0), 1);
                        xhci_rt_write(XHCI_RT_ERSTBA(0), (uint32_t)vmm_get_phys(erst));
                        xhci_rt_write(XHCI_RT_ERSTBA(0) + 4, (uint32_t)(vmm_get_phys(erst) >> 32));
                        xhci_rt_write(XHCI_RT_ERDP(0), (uint32_t)vmm_get_phys(event_ring));
                        xhci_rt_write(XHCI_RT_ERDP(0) + 4, (uint32_t)(vmm_get_phys(event_ring) >> 32));

                        xhci_op_write(XHCI_OP_USBCMD, xhci_op_read(XHCI_OP_USBCMD) | 0x01);
                        while(xhci_op_read(XHCI_OP_USBSTS) & 0x01);
                        serial_write_str("[XHCI] Online.\n");
                        return;
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
#endif
}
