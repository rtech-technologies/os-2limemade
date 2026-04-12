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
void pit_wait_ms(uint32_t ms);

static void* xhci_base = NULL;
static uint32_t xhci_cap_len = 0;
static uint32_t xhci_rt_off = 0;

static xhci_trb_t* cmd_ring = NULL;
static xhci_trb_t* event_ring = NULL;
static xhci_erst_entry_t* erst = NULL;
static uint64_t* dcbaap = NULL;

static int cmd_ring_idx = 0;
static bool cmd_cycle = true;
static int event_idx = 0;
static bool event_cycle = true;
static volatile int last_cmd_status = -1;
static volatile int last_cmd_slot = -1;

static xhci_trb_t* ep0_rings[64];

static xhci_trb_t* xhci_alloc_ring(void) {
    void* slab_alloc_aligned(int id, size_t size, size_t align);
    xhci_trb_t* ring = slab_alloc_aligned(0, 4096, 64);
    if (!ring) return NULL;
    for (int i = 0; i < 256; i++) {
        ring[i].ptr = 0; ring[i].status = 0; ring[i].control = 0;
    }
    return ring;
}

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

static inline void xhci_db_write(uint32_t reg, uint32_t val) {
    uint32_t dboff = *(volatile uint32_t*)((uint8_t*)xhci_base + XHCI_CAP_DBOFF);
    *(volatile uint32_t*)((uint8_t*)xhci_base + dboff + reg) = val;
}

static inline void xhci_op_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)((uint8_t*)xhci_base + xhci_cap_len + reg) = val;
}

static inline uint32_t xhci_op_read(uint32_t reg) {
    return *(volatile uint32_t*)((uint8_t*)xhci_base + xhci_cap_len + reg);
}

void kbd_push(char c);

void xhci_handle_events(void) {
    if (!xhci_base || !event_ring) return;

    /* Process all available events in the ring */
    while ((event_ring[event_idx].control & 0x01) == (uint32_t)event_cycle) {
        xhci_trb_t* ev = &event_ring[event_idx];
        uint8_t type = (ev->control >> 10) & 0x3F;

        if (type == TRB_TYPE_TRANSFER_EV) {
            /* TRB Pointer translated via HHDM */
            uint64_t report_phys = ev->ptr;
            uint8_t* report = (uint8_t*)(report_phys + get_hhdm_offset());
            if (report) {
                if (report[1] == 0 && report[2] != 0) { /* Keyboard */
                    uint8_t code = report[2];
                    kbd_push(usb_map[code]);
                } else { /* Mouse */
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
        } else if (type == TRB_TYPE_CMD_COMP_EV) {
            last_cmd_status = (ev->status >> 24) & 0xFF;
            last_cmd_slot = (ev->control >> 24) & 0xFF;
        } else if (type == TRB_TYPE_PORT_STATUS_EV) {
            uint8_t port_id = (ev->ptr >> 24) & 0xFF;
            serial_print("[XHCI] Port Status Change on Port %d\n", port_id);
        }

        /* Update Dequeue Pointer: Inform xHC of the last Event TRB processed */
        uint64_t erdp_phys = vmm_get_phys(&event_ring[event_idx]);
        xhci_rt_write(XHCI_RT_ERDP(0), (uint32_t)erdp_phys | 0x08);
        xhci_rt_write(XHCI_RT_ERDP(0) + 4, (uint32_t)(erdp_phys >> 32));

        /* Advance Event Ring */
        event_idx = (event_idx + 1) % 256;
        if (event_idx == 0) event_cycle = !event_cycle;
    }
}

int xhci_send_command(xhci_trb_t* trb) {
    if (!cmd_ring) return -1;

    last_cmd_status = -1;
    last_cmd_slot = -1;

    int idx = cmd_ring_idx;
    cmd_ring[idx].ptr = trb->ptr;
    cmd_ring[idx].status = trb->status;
    uint32_t control = (trb->control & ~0x01) | (cmd_cycle ? 1 : 0);
    cmd_ring[idx].control = control;

    cmd_ring_idx++;
    if (cmd_ring_idx == 255) {
        cmd_ring[255].ptr = vmm_get_phys(cmd_ring);
        cmd_ring[255].status = 0;
        cmd_ring[255].control = (6 << 10) | (1 << 1) | (cmd_cycle ? 1 : 0);
        cmd_ring_idx = 0;
        cmd_cycle = !cmd_cycle;
    }

    xhci_db_write(0, 0); /* Ring Doorbell 0 */

    /* Wait for Completion via unified handler */
    int timeout = 1000;
    while(timeout--) {
        xhci_handle_events();
        if (last_cmd_status != -1) return (last_cmd_status == 1) ? 0 : -1;
        void pit_wait_ms(uint32_t ms);
        pit_wait_ms(1);
    }
    return -1;
}

static xhci_dev_ctx_t* dev_contexts[64];

void xhci_setup_device(int port) {
    serial_print("[XHCI] Initializing Device on Port %d\n", port);

    /* 1. Enable Slot */
    xhci_trb_t cmd = {0};
    cmd.control = (TRB_TYPE_ENABLE_SLOT << 10);
    if (xhci_send_command(&cmd) != 0) {
        serial_write_str("[XHCI] Error: ENABLE_SLOT failed.\n");
        return;
    }

    int slot_id = last_cmd_slot;
    if (slot_id <= 0) return;
    serial_print("[XHCI] Slot %d enabled.\n", slot_id);

    /* 2. Setup Device Context and Address Device */
    void* slab_alloc_aligned(int id, size_t size, size_t align);
    dev_contexts[slot_id] = slab_alloc_aligned(0, sizeof(xhci_dev_ctx_t), 64);
    if (!dev_contexts[slot_id]) return;
    for(int i=0; i<(int)sizeof(xhci_dev_ctx_t)/4; i++) ((uint32_t*)dev_contexts[slot_id])[i] = 0;
    dcbaap[slot_id] = vmm_get_phys(dev_contexts[slot_id]);

    /* Allocate EP0 Transfer Ring */
    ep0_rings[slot_id] = xhci_alloc_ring();

    /* Prepare Input Context */
    xhci_input_ctx_t* ictx = slab_alloc_aligned(0, sizeof(xhci_input_ctx_t), 64);
    if (!ictx) return;
    for(int i=0; i<(int)sizeof(xhci_input_ctx_t)/4; i++) ((uint32_t*)ictx)[i] = 0;

    /* xHCI Hardware Mandate: Input Context MUST be zeroed before use */
    ictx->drop_flags = 0;
    ictx->add_flags = 0x03; /* Slot and EP0 */

    /* Get Port Speed from PORTSC */
    uint32_t port_reg = 0x400 + (port * 0x10);
    uint32_t portsc = xhci_op_read(port_reg);
    uint32_t speed = (portsc >> 10) & 0x0F;

    /* Slot Context:
       - Context Entries = 1 (EP0)
       - Speed (bits 20-23)
       - Root Port Num (bits 16-23 in Slot Context Info 1)
       - Max Exit Latency = 0 (bits 0-15 in Slot Context Info 1)
    */
    ictx->slot.info[0] = (1 << 27) | (speed << 20);
    ictx->slot.info[1] = ((port + 1) << 16); /* Root Hub Port Number, Latency=0 */

    /* EP0 Context (Control Endpoint) */
    uint32_t max_packet_size = 8;
    if (speed == 3) max_packet_size = 64; // High Speed
    else if (speed == 4) max_packet_size = 512; // Super Speed

    uint64_t ep_phys = vmm_get_phys(ep0_rings[slot_id]);
    ictx->ep[0].info[1] = (4 << 3) | (max_packet_size << 16); /* Type: Control, Max Packet */
    ictx->ep[0].tr_ptr = ep_phys | 1; /* Dequeue Pointer + DCS=1 */

    cmd.ptr = vmm_get_phys(ictx);
    cmd.control = (TRB_TYPE_ADDRESS_DEVICE << 10) | (slot_id << 24);
    if (xhci_send_command(&cmd) == 0) {
        serial_print("[XHCI] Device Address assigned to Slot %d (Speed %d).\n", slot_id, speed);
    } else {
        serial_print("[XHCI] Error: ADDRESS_DEVICE failed for Slot %d.\n", slot_id);
    }
}

void vga_print(const char* s);

void usb_main_task(void) {
#if defined(CONFIG_INTERFACE_PS2)
    while(1) sys_yield();
#endif

    if (!xhci_base) {
        while(1) sys_yield();
    }

    vga_print("[USB] Sovereign Task Active.\n");
    uint32_t hcsparams1 = *(volatile uint32_t*)((uint8_t*)xhci_base + 0x04);
    uint32_t port_count = hcsparams1 >> 24;

    while(1) {
        /* 1. Device Connection Monitor */
        for(uint32_t i=0; i<port_count; i++) {
            uint32_t port_reg = 0x400 + (i * 0x10);
            uint32_t portsc = xhci_op_read(port_reg);

            if (portsc & 0x01) { /* Current Connect Status (CCS) */
                if (portsc & (1 << 17)) { /* Connect Status Change (CSC) */
                    /* Clear CSC by writing 1 to it while preserving other W1C bits as 0 */
                    xhci_op_write(port_reg, (portsc & 0xFFFF0000) | (1 << 17));
                }

                if (!(portsc & 0x02)) { /* Port Enabled/Disabled (PED) */
                    serial_print("[XHCI] Port %d connected but disabled. Hard Resetting...\n", i);
                    /* Trigger Reset (PR) */
                    xhci_op_write(port_reg, (portsc & 0xFFFF0000) | (1 << 4));

                    /* Wait for Port Reset Change (PRC) or PED bit */
                    int timeout = 50;
                    while (timeout--) {
                        void pit_wait_ms(uint32_t ms);
                        pit_wait_ms(10);
                        portsc = xhci_op_read(port_reg);
                        if (portsc & (1 << 21)) break; /* PRC */
                    }

                    /* Clear PRC */
                    xhci_op_write(port_reg, (portsc & 0xFFFF0000) | (1 << 21));

                    /* Re-read status to verify PED */
                    portsc = xhci_op_read(port_reg);
                    if (portsc & 0x02) {
                        serial_print("[XHCI] Port %d enabled. Initializing device...\n", i);
                        xhci_setup_device(i);
                    } else {
                        serial_print("[XHCI] Port %d reset failed (PED=0).\n", i);
                    }
                }
            }
        }

        /* 2. Event Ring Processor */
        xhci_handle_events();

        /* Handover */
        sys_yield();
    }
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
                    if (((class_info >> 24) & 0xFF) == 0x0C && ((class_info >> 16) & 0xFF) == 0x03) {
                        uint8_t prog_if = (class_info >> 8) & 0xFF;
                        serial_print("[PCI] USB Controller Found: %d:%d:%d (ProgIF: 0x%x)\n", bus, slot, func, prog_if);

                        if (prog_if == 0x30) { /* xHCI (USB 3.0) */
                            pci_enable_master(bus, slot, func);
                            uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                            xhci_base = (void*)(get_hhdm_offset() + (uint64_t)(bar0 & 0xFFFFFFF0));

                            /* Hardware Presence Validation */
                            if (*(volatile uint32_t*)xhci_base == 0xFFFFFFFF) {
                                serial_write_str("[XHCI] Error: Hardware reported 0xFFFFFFFF (Ghost Device). Aborting.\n");
                                xhci_base = NULL;
                                continue;
                            }

                            xhci_bios_handover(bus, slot, func, xhci_base);

                            xhci_cap_len = *(volatile uint8_t*)xhci_base;
                            if (xhci_cap_len == 0xFF) {
                                serial_write_str("[XHCI] Error: Invalid Capability Length. Aborting.\n");
                                xhci_base = NULL;
                                continue;
                            }
                            xhci_rt_off = *(volatile uint32_t*)((uint8_t*)xhci_base + XHCI_CAP_RTSOFF);

                            /* Stop and Reset Controller */
                            xhci_op_write(XHCI_OP_USBCMD, xhci_op_read(XHCI_OP_USBCMD) & ~0x01);
                            int timeout = 1000;
                            while(!(xhci_op_read(XHCI_OP_USBSTS) & 0x01) && timeout--) pit_wait_ms(1);

                            xhci_op_write(XHCI_OP_USBCMD, 0x02);
                            timeout = 1000;
                            while((xhci_op_read(XHCI_OP_USBCMD) & 0x02) && timeout--) pit_wait_ms(1);

                            void* slab_alloc_aligned(int id, size_t size, size_t align);
                            cmd_ring = slab_alloc_aligned(0, 4096, 64);
                            event_ring = slab_alloc_aligned(0, 4096, 64);
                            erst = slab_alloc_aligned(0, 4096, 64);
                            dcbaap = slab_alloc_aligned(0, 4096, 64);
                            for(int k=0; k<1024; k++) dcbaap[k] = 0;

                            /* Scratchpad Buffers */
                            uint32_t hcsparams2 = *(volatile uint32_t*)((uint8_t*)xhci_base + 0x08);
                            uint32_t max_scratchpads = (hcsparams2 >> 21) & 0x1F;
                            if (max_scratchpads > 0) {
                                uint64_t* scratch_array = slab_alloc_aligned(0, max_scratchpads * 8, 64);
                                for (uint32_t k = 0; k < max_scratchpads; k++) {
                                    void* buf = slab_alloc_aligned(0, 4096, 4096);
                                    scratch_array[k] = vmm_get_phys(buf);
                                }
                                dcbaap[0] = vmm_get_phys(scratch_array);
                            }

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
                            xhci_rt_write(XHCI_RT_ERDP(0), (uint32_t)vmm_get_phys(event_ring) | 0x08);
                            xhci_rt_write(XHCI_RT_ERDP(0) + 4, (uint32_t)(vmm_get_phys(event_ring) >> 32));

                            xhci_op_write(XHCI_OP_USBCMD, xhci_op_read(XHCI_OP_USBCMD) | 0x01);
                            timeout = 1000;
                            while((xhci_op_read(XHCI_OP_USBSTS) & 0x01) && timeout--) pit_wait_ms(1);
                            serial_write_str("[XHCI] Online.\n");
                            return;
                        }
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
#endif
}
