#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <include/xhci.h>
#include <include/usb.h>
#include <include/mouse.h>
#include <include/config.h>
#include <include/usb_queues.h>

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
static volatile int last_transfer_status[64][32];

typedef enum {
    USB_TYPE_UNKNOWN,
    USB_TYPE_HUB,
    USB_TYPE_KBD,
    USB_TYPE_MOUSE,
    USB_TYPE_MSC
} usb_device_type_t;

typedef struct {
    uint8_t num;
    uint8_t type; /* 1=Isoch, 2=Bulk, 3=Interrupt */
    uint8_t dir;  /* 0=Out, 1=In */
    uint16_t max_packet;
    uint8_t interval;
} usb_endpoint_info_t;

typedef struct {
    int slot_id;
    int port;
    usb_device_type_t type;
    usb_device_descriptor_t desc;
    usb_endpoint_info_t eps[31];
    int ep_count;
    int bulk_in_idx;
    int bulk_out_idx;
    uint8_t last_report[8];
    bool is_tablet;
} xhci_device_t;

static xhci_device_t usb_devices[64];

static xhci_trb_t* ep_rings[64][32];

static hid_queue_t kbd_report_queue = { .head = 0, .tail = 0 };
static hid_queue_t mouse_report_queue = { .head = 0, .tail = 0 };

void console_push_char(char c);


/* Scancode Map: USB HID to ASCII (Partial) */
static char usb_map[256] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd', [0x08] = 'e', [0x09] = 'f',
    [0x0A] = 'g', [0x0B] = 'h', [0x0C] = 'i', [0x0D] = 'j', [0x0E] = 'k', [0x0F] = 'l',
    [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p', [0x14] = 'q', [0x15] = 'r',
    [0x16] = 's', [0x17] = 't', [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x',
    [0x1C] = 'y', [0x1D] = 'z', [0x1E] = '1', [0x1F] = '2', [0x20] = '3', [0x21] = '4',
    [0x22] = '5', [0x23] = '6', [0x24] = '7', [0x25] = '8', [0x26] = '9', [0x27] = '0',
    [0x28] = '\n', [0x2C] = ' ', [0x2A] = '\b', [0x2B] = '\t', [0x2D] = '-', [0x2E] = '=',
    [0x2F] = '[', [0x30] = ']', [0x31] = '\\', [0x33] = ';', [0x34] = '\'', [0x36] = ',',
    [0x37] = '.', [0x38] = '/', [0x29] = 27 /* Esc */, [0x4C] = 127 /* Del */,
    [0x35] = '`'
};

static char usb_shift_map[256] = {
    [0x04] = 'A', [0x05] = 'B', [0x06] = 'C', [0x07] = 'D', [0x08] = 'E', [0x09] = 'F',
    [0x0A] = 'G', [0x0B] = 'H', [0x0C] = 'I', [0x0D] = 'J', [0x0E] = 'K', [0x0F] = 'L',
    [0x10] = 'M', [0x11] = 'N', [0x12] = 'O', [0x13] = 'P', [0x14] = 'Q', [0x15] = 'R',
    [0x16] = 'S', [0x17] = 'T', [0x18] = 'U', [0x19] = 'V', [0x1A] = 'W', [0x1B] = 'X',
    [0x1C] = 'Y', [0x1D] = 'Z', [0x1E] = '!', [0x1F] = '@', [0x20] = '#', [0x21] = '$',
    [0x22] = '%', [0x23] = '^', [0x24] = '&', [0x25] = '*', [0x26] = '(', [0x27] = ')',
    [0x28] = '\n', [0x2C] = ' ', [0x2A] = '\b', [0x2B] = '\t', [0x2D] = '_', [0x2E] = '+',
    [0x2F] = '{', [0x30] = '}', [0x31] = '|', [0x33] = ':', [0x34] = '"', [0x36] = '<',
    [0x37] = '>', [0x38] = '?', [0x29] = 27, [0x4C] = 127, [0x35] = '~'
};

uint8_t repeat_key = 0;
uint8_t repeat_modifiers = 0;
uint64_t next_repeat_tick = 0;

int xhci_transfer(int slot, int dci, xhci_trb_t* trb);
uint64_t get_system_ticks(void);

void xhci_handle_repeat(void) {
    if (repeat_key == 0) return;
    uint64_t now = get_system_ticks();
    if (now >= next_repeat_tick) {
        bool shift = (repeat_modifiers & 0x02) || (repeat_modifiers & 0x20);
        char c = shift ? usb_shift_map[repeat_key] : usb_map[repeat_key];
        if (c) console_push_char(c);
        next_repeat_tick = now + 50; /* 50ms repeat rate */
    }
}

static xhci_trb_t* xhci_alloc_ring(void) {
    void* slab_alloc_aligned(int id, size_t size, size_t align);
    xhci_trb_t* ring = slab_alloc_aligned(0, 4096, 64);
    if (!ring) return NULL;
    for (int i = 0; i < 256; i++) {
        ring[i].ptr = 0; ring[i].status = 0; ring[i].control = 0;
    }
    return ring;
}


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

void console_push_char(char c);

void xhci_handle_events(void) {
    if (!xhci_base || !event_ring) return;

    /* Process all available events in the ring */
    while ((event_ring[event_idx].control & 0x01) == (uint32_t)event_cycle) {
        xhci_trb_t* ev = &event_ring[event_idx];
        uint8_t type = (ev->control >> 10) & 0x3F;

        if (type == TRB_TYPE_TRANSFER_EV) {
            int slot_id = (ev->control >> 24) & 0xFF;
            int dci = (ev->control >> 16) & 0x1F;
            last_transfer_status[slot_id][dci] = (ev->status >> 24) & 0xFF;

            /* XHCI Protocol: ev->ptr is the Physical Address of the COMPLETED TRB */
            /* We MUST use the HHDM offset to access the TRB in virtual space */
            xhci_trb_t* completed_trb = (xhci_trb_t*)(ev->ptr + get_hhdm_offset());

            /* The TRB's ptr field contains the physical address of the data buffer (HID report) */
            uint64_t report_phys = completed_trb->ptr;
            uint8_t* report = (uint8_t*)(report_phys + get_hhdm_offset());

            if (report_phys != 0 && report) {
                hid_queue_t* target = NULL;
                if (usb_devices[slot_id].type == USB_TYPE_KBD) target = &kbd_report_queue;
                else if (usb_devices[slot_id].type == USB_TYPE_MOUSE) target = &mouse_report_queue;

                if (target) {
                    int next = (target->tail + 1) % HID_QUEUE_SIZE;
                    if (next != target->head) {
                        for(int k=0; k<8; k++) target->reports[target->tail].data[k] = report[k];
                        target->reports[target->tail].slot = slot_id;
                        target->reports[target->tail].dci = dci;
                        target->tail = next;
                    }
                }

                /* Re-queue the Interrupt In TRB */
                xhci_trb_t t_trb = {0};
                t_trb.ptr = report_phys;
                t_trb.status = 8;
                t_trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC=1 */
                xhci_transfer(slot_id, dci, &t_trb);
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
        /* EHB (Event Handler Busy) bit (bit 3) must be set to clear interrupt */
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
        if (last_cmd_status != -1) return (last_cmd_status == XHCI_COMP_SUCCESS) ? 0 : (int)last_cmd_status;
        void pit_wait_ms(uint32_t ms);
        pit_wait_ms(1);
    }
    return -1;
}

int xhci_ring_doorbell(int slot, int dball) {
    xhci_db_write(slot * 4, dball);
    return 0;
}

static int ring_indices[64][32];
static bool ring_cycles[64][32];
static bool rings_init = false;

int xhci_transfer(int slot, int dci, xhci_trb_t* trb) {
    if (!rings_init) {
        for(int s=0; s<64; s++) {
            for(int e=0; e<32; e++) {
                ring_cycles[s][e] = true;
                last_transfer_status[s][e] = -1;
            }
        }
        rings_init = true;
    }

    last_transfer_status[slot][dci] = -1;
    xhci_trb_t* ring = ep_rings[slot][dci];
    if (!ring) return -1;

    int idx = ring_indices[slot][dci];
    ring[idx].ptr = trb->ptr;
    ring[idx].status = trb->status;
    ring[idx].control = (trb->control & ~0x01) | (ring_cycles[slot][dci] ? 1 : 0);

    ring_indices[slot][dci] = (idx + 1) % 256;
    if (ring_indices[slot][dci] == 255) {
        /* Link TRB to loop back */
        ring[255].ptr = vmm_get_phys(ring);
        ring[255].status = 0;
        ring[255].control = (6 << 10) | (1 << 1) | (ring_cycles[slot][dci] ? 1 : 0);
        ring_indices[slot][dci] = 0;
        ring_cycles[slot][dci] = !ring_cycles[slot][dci];
    }

    xhci_ring_doorbell(slot, dci);
    return 0;
}

void xhci_reset_endpoint(int slot, int dci) {
    xhci_trb_t cmd = {0};
    cmd.control = (TRB_TYPE_RESET_EP << 10) | (slot << 24) | (dci << 16);
    xhci_send_command(&cmd);
}

int xhci_wait_transfer(int slot, int dci) {
    int timeout = 1000;
    while(timeout--) {
        xhci_handle_events();
        int status = last_transfer_status[slot][dci];
        if (status != -1) {
            if (status == XHCI_COMP_SUCCESS) return 0;
            if (status == XHCI_COMP_STALL_ERR) {
                serial_print("[XHCI] Stall on Slot %d DCI %d. Resetting...\n", slot, dci);
                xhci_reset_endpoint(slot, dci);
            }
            return (int)status;
        }
        pit_wait_ms(1);
    }
    return -1;
}

int xhci_control_transfer(int slot, usb_setup_packet_t* setup, void* data, int len) {
    xhci_trb_t trb = {0};

    /* 1. Setup Stage */
    trb.ptr = *(uint64_t*)setup;
    trb.status = 8; /* Setup length */
    trb.control = (TRB_TYPE_SETUP_STAGE << 10) | (1 << 6); /* IDT=1 */
    /* TRT (Transfer Type): 0=No Data, 2=Data Out, 3=Data In */
    if (len > 0) {
        if (setup->bmRequestType & 0x80) trb.control |= (3 << 14);
        else trb.control |= (2 << 14);
    }

    xhci_transfer(slot, 1, &trb); /* EP0 is DCI 1 */

    /* 2. Data Stage */
    if (len > 0) {
        trb.ptr = vmm_get_phys(data);
        trb.status = len;
        trb.control = (TRB_TYPE_DATA_STAGE << 10);
        if (setup->bmRequestType & 0x80) trb.control |= (1 << 16); /* DIR=In */
        xhci_transfer(slot, 1, &trb);
    }

    /* 3. Status Stage */
    trb.ptr = 0;
    trb.status = 0;
    trb.control = (TRB_TYPE_STATUS_STAGE << 10) | (1 << 5); /* IOC=1 */
    if (len > 0 && (setup->bmRequestType & 0x80)) {
        /* If data was In, status is Out */
    } else if (len > 0) {
        trb.control |= (1 << 16); /* DIR=In */
    } else {
        trb.control |= (1 << 16); /* DIR=In for No Data */
    }

    xhci_transfer(slot, 1, &trb);
    return xhci_wait_transfer(slot, 1);
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

    /* Allocate EP0 Transfer Ring (DCI 1) */
    ep_rings[slot_id][1] = xhci_alloc_ring();

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

    /* EP0 Context (Control Endpoint, DCI 1) */
    uint32_t max_packet_size = 8;
    if (speed == 3) max_packet_size = 64; // High Speed
    else if (speed == 4) max_packet_size = 512; // Super Speed

    uint64_t ep_phys = vmm_get_phys(ep_rings[slot_id][1]);
    ictx->ep[0].info[1] = (4 << 3) | (max_packet_size << 16); /* Type: Control, Max Packet */
    ictx->ep[0].tr_ptr = ep_phys | 1; /* Dequeue Pointer + DCS=1 */

    cmd.ptr = vmm_get_phys(ictx);
    cmd.control = (TRB_TYPE_ADDRESS_DEVICE << 10) | (slot_id << 24);
    if (xhci_send_command(&cmd) == 0) {
        serial_print("[XHCI] Device Address assigned to Slot %d (Speed %d).\n", slot_id, speed);

        /* Fetch Device Descriptor */
        usb_device_descriptor_t ddesc = {0};
        usb_setup_packet_t setup = {0};
        setup.bmRequestType = 0x80;
        setup.bRequest = USB_REQ_GET_DESCRIPTOR;
        setup.wValue = (USB_DESC_DEVICE << 8);
        setup.wLength = sizeof(usb_device_descriptor_t);

        if (xhci_control_transfer(slot_id, &setup, &ddesc, sizeof(ddesc)) == 0) {
            serial_print("[USB] Device Found: Vendor=0x%x Product=0x%x\n", ddesc.idVendor, ddesc.idProduct);
            usb_devices[slot_id].slot_id = slot_id;
            usb_devices[slot_id].port = port;
            usb_devices[slot_id].desc = ddesc;

            /* Fetch Configuration Descriptor (Header only first) */
            usb_config_descriptor_t cdesc = {0};
            setup.bRequest = USB_REQ_GET_DESCRIPTOR;
            setup.wValue = (USB_DESC_CONFIG << 8);
            setup.wLength = sizeof(cdesc);
            if (xhci_control_transfer(slot_id, &setup, &cdesc, sizeof(cdesc)) == 0) {
                /* Fetch full Configuration Tree */
                uint8_t* full_cfg = slab_alloc_aligned(0, cdesc.wTotalLength, 64);
                setup.wLength = cdesc.wTotalLength;
                if (xhci_control_transfer(slot_id, &setup, full_cfg, cdesc.wTotalLength) == 0) {
                    serial_print("[USB] Configuration loaded (%d bytes).\n", cdesc.wTotalLength);

                    /* Parse for Interfaces and Endpoints */
                    uint8_t* ptr = full_cfg;
                    usb_devices[slot_id].ep_count = 0;
                    int current_iface = -1;
                    while (ptr < full_cfg + cdesc.wTotalLength) {
                        uint8_t len = ptr[0];
                        uint8_t type = ptr[1];
                        if (type == 0x04) { /* Interface */
                            usb_interface_descriptor_t* iface = (usb_interface_descriptor_t*)ptr;
                            current_iface = iface->bInterfaceNumber;
                            if (iface->bInterfaceClass == 0x03) { /* HID */
                                if (iface->bInterfaceProtocol == 1) {
                                    serial_write_str("[USB] Identified KEYBOARD.\n");
                                    usb_devices[slot_id].type = USB_TYPE_KBD;
                                } else if (iface->bInterfaceProtocol == 2) {
                                    serial_write_str("[USB] Identified MOUSE.\n");
                                    usb_devices[slot_id].type = USB_TYPE_MOUSE;
                                }
                            } else if (iface->bInterfaceClass == 0x08) { /* MSC */
                                serial_write_str("[USB] Identified MASS STORAGE.\n");
                                usb_devices[slot_id].type = USB_TYPE_MSC;
                            }
                        } else if (type == 0x21) { /* HID Descriptor */
                            usb_hid_descriptor_t* hid = (usb_hid_descriptor_t*)ptr;
                            serial_print("[USB] HID Descriptor found. Report Len: %d\n", hid->wDescriptorLength);
                            /* Request Report Descriptor to kickstart some hardware */
                            uint8_t* rdesc = slab_alloc_aligned(0, hid->wDescriptorLength, 64);
                            usb_setup_packet_t s_hid = {0};
                            s_hid.bmRequestType = 0x81; /* Interface */
                            s_hid.bRequest = USB_REQ_GET_DESCRIPTOR;
                            s_hid.wValue = (USB_DESC_REPORT << 8);
                            s_hid.wIndex = current_iface;
                            s_hid.wLength = hid->wDescriptorLength;
                            xhci_control_transfer(slot_id, &s_hid, rdesc, hid->wDescriptorLength);
                        } else if (type == 0x05) { /* Endpoint */
                            usb_endpoint_descriptor_t* ep = (usb_endpoint_descriptor_t*)ptr;
                            int ec = usb_devices[slot_id].ep_count;
                            if (ec < 31) {
                                usb_devices[slot_id].eps[ec].num = ep->bEndpointAddress & 0x0F;
                                usb_devices[slot_id].eps[ec].dir = (ep->bEndpointAddress & 0x80) ? 1 : 0;
                                usb_devices[slot_id].eps[ec].type = ep->bmAttributes & 0x03;
                                usb_devices[slot_id].eps[ec].max_packet = ep->wMaxPacketSize;
                                usb_devices[slot_id].eps[ec].interval = ep->bInterval;
                                usb_devices[slot_id].ep_count++;
                            }
                        }
                        ptr += len;
                    }

                    /* Set Configuration 1 */
                    setup.bmRequestType = 0x00;
                    setup.bRequest = USB_REQ_SET_CONFIGURATION;
                    setup.wValue = 1;
                    setup.wIndex = 0;
                    setup.wLength = 0;
                    xhci_control_transfer(slot_id, &setup, NULL, 0);

                    /* Configure Endpoints in xHC context */
                    xhci_input_ctx_t* c_ictx = slab_alloc_aligned(0, sizeof(xhci_input_ctx_t), 64);
                    for(int i=0; i<(int)sizeof(xhci_input_ctx_t)/4; i++) ((uint32_t*)c_ictx)[i] = 0;

                    c_ictx->add_flags = 0x01; /* Slot Context always added */

                    /* Copy current Slot Context */
                    c_ictx->slot = dev_contexts[slot_id]->slot;
                    int max_ep = 0;

                    for (int i = 0; i < usb_devices[slot_id].ep_count; i++) {
                        usb_endpoint_info_t* ep_info = &usb_devices[slot_id].eps[i];
                        int dci = (ep_info->num * 2) + (ep_info->dir == 1 ? 1 : 0);
                        int ep_ctx_idx = dci - 1;
                        if (ep_ctx_idx < 1 || ep_ctx_idx >= 31) continue;

                        c_ictx->add_flags |= (1 << dci);
                        ep_rings[slot_id][dci] = xhci_alloc_ring();

                        uint32_t type = 0;
                        if (ep_info->type == 2) type = (ep_info->dir == 1) ? 6 : 2; /* Bulk In/Out */
                        else if (ep_info->type == 3) type = (ep_info->dir == 1) ? 7 : 3; /* Interrupt In/Out */

                        c_ictx->ep[ep_ctx_idx].info[1] = (type << 3) | (ep_info->max_packet << 16) | (ep_info->interval << 8);
                        c_ictx->ep[ep_ctx_idx].tr_ptr = vmm_get_phys(ep_rings[slot_id][dci]) | 1;

                        if (ep_info->type == 2) {
                            if (ep_info->dir == 1) usb_devices[slot_id].bulk_in_idx = dci;
                            else usb_devices[slot_id].bulk_out_idx = dci;
                        }

                        if (dci > max_ep) max_ep = dci;
                    }

                    /* Update Context Entries in Slot Context */
                    c_ictx->slot.info[0] = (c_ictx->slot.info[0] & ~(0x1F << 27)) | ((max_ep + 1) << 27);

                    xhci_trb_t c_cmd = {0};
                    c_cmd.ptr = vmm_get_phys(c_ictx);
                    c_cmd.control = (TRB_TYPE_CONFIG_EP << 10) | (slot_id << 24);
                    if (xhci_send_command(&c_cmd) == 0) {
                        serial_print("[USB] Endpoints configured for Slot %d.\n", slot_id);

                        /* Preliminary MSC Discovery */
                        if (usb_devices[slot_id].type == USB_TYPE_MSC) {
                            int xhci_msc_read(void* priv, uint64_t lba, uint32_t count, void* buffer);

                            /* Fetch Capacity */
                            uint8_t cap_buf[8];
                            usb_msc_cbw_t cbw = {0};
                            cbw.dCBWSignature = MSC_CBW_SIGNATURE;
                            cbw.dCBWTag = 0x12345678;
                            cbw.dCBWDataTransferLength = 8;
                            cbw.bmCBWFlags = 0x80; /* In */
                            cbw.bCBWLUN = 0;
                            cbw.bCBWCBLength = 10;
                            cbw.CBWCB[0] = 0x25; /* READ CAPACITY(10) */

                            xhci_trb_t t_trb = {0};
                            t_trb.ptr = vmm_get_phys(&cbw);
                            t_trb.status = sizeof(cbw);
                            t_trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5);
                            xhci_transfer(slot_id, usb_devices[slot_id].bulk_out_idx, &t_trb);
                            xhci_wait_transfer(slot_id, usb_devices[slot_id].bulk_out_idx);

                            t_trb.ptr = vmm_get_phys(cap_buf);
                            t_trb.status = 8;
                            t_trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5);
                            xhci_transfer(slot_id, usb_devices[slot_id].bulk_in_idx, &t_trb);
                            xhci_wait_transfer(slot_id, usb_devices[slot_id].bulk_in_idx);

                            usb_msc_csw_t csw = {0};
                            t_trb.ptr = vmm_get_phys(&csw);
                            t_trb.status = sizeof(csw);
                            t_trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5);
                            xhci_transfer(slot_id, usb_devices[slot_id].bulk_in_idx, &t_trb);
                            xhci_wait_transfer(slot_id, usb_devices[slot_id].bulk_in_idx);

                            uint32_t max_lba = (cap_buf[0] << 24) | (cap_buf[1] << 16) | (cap_buf[2] << 8) | cap_buf[3];

                            int xhci_msc_write(void* priv, uint64_t lba, uint32_t count, void* buffer);
                            /* Register MSC as Physical Volume */
                            vdisk_node_t msc = {
                                .name = "USB_STICK",
                                .sector_size = 512,
                                .total_lba = max_lba + 1,
                                .partition_offset = 0,
                                .read_lba = xhci_msc_read,
                                .write_lba = xhci_msc_write,
                                .is_atapi = false,
                                .private_data = (void*)(uint64_t)slot_id
                            };
                            void register_hardware_disk(vdisk_node_t node);
                            register_hardware_disk(msc);
                        }

                        /* Kick off Interrupt In transfers for HID */
                        if (usb_devices[slot_id].type == USB_TYPE_KBD || usb_devices[slot_id].type == USB_TYPE_MOUSE) {
                            for (int i = 0; i < usb_devices[slot_id].ep_count; i++) {
                                usb_endpoint_info_t* ep_info = &usb_devices[slot_id].eps[i];
                                if (ep_info->type == 3 && ep_info->dir == 1) { /* Interrupt In */
                                    int dci = (ep_info->num * 2) + 1;
                                    void* report_buf = slab_alloc_aligned(0, ep_info->max_packet, 64);
                                    xhci_trb_t t_trb = {0};
                                    t_trb.ptr = vmm_get_phys(report_buf);
                                    t_trb.status = ep_info->max_packet;
                                    t_trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC=1 */
                                    xhci_transfer(slot_id, dci, &t_trb);
                                    serial_print("[USB] Interrupt In started for Slot %d DCI %d\n", slot_id, dci);
                                }
                            }
                        }
                    } else {
                        serial_print("[USB] Error: CONFIG_EP failed for Slot %d.\n", slot_id);
                    }
                }
            }
        }
    } else {
        serial_print("[XHCI] Error: ADDRESS_DEVICE failed for Slot %d.\n", slot_id);
    }
}

void vga_print(const char* s);
void vga_draw_mouse(int x, int y);

void usb_keyboard_task(void) {
    vga_print("[USB] Keyboard Task Active.\n");
    while(1) {
        while(kbd_report_queue.head != kbd_report_queue.tail) {
            hid_report_t* report_obj = &kbd_report_queue.reports[kbd_report_queue.head];
            uint8_t* report = report_obj->data;
            int slot_id = report_obj->slot;

            uint8_t modifiers = report[0];
            extern bool control_pressed;
            control_pressed = (modifiers & 0x01) || (modifiers & 0x10);

            /* Check for releases of the repeat key */
            if (repeat_key != 0) {
                bool still_pressed = false;
                for (int i = 2; i < 8; i++) {
                    if (report[i] == repeat_key) {
                        still_pressed = true;
                        break;
                    }
                }
                if (!still_pressed) {
                    repeat_key = 0;
                } else {
                    repeat_modifiers = modifiers;
                }
            }

            /* Check for new key presses */
            for (int i = 2; i < 8; i++) {
                uint8_t code = report[i];
                if (code == 0) continue;

                bool was_pressed = false;
                for (int j = 2; j < 8; j++) {
                    if (usb_devices[slot_id].last_report[j] == code) {
                        was_pressed = true;
                        break;
                    }
                }

                if (!was_pressed) {
                    if (control_pressed && code == 0x06) { /* 'c' */
                        void console_copy_selection(void);
                        console_copy_selection();
                    }

                    bool shift = (modifiers & 0x02) || (modifiers & 0x20);
                    char c = shift ? usb_shift_map[code] : usb_map[code];
                    if (c) {
                        console_push_char(c);
                        repeat_key = code;
                        repeat_modifiers = modifiers;
                        next_repeat_tick = get_system_ticks() + 500;
                    }
                }
            }
            for (int i = 0; i < 8; i++) usb_devices[slot_id].last_report[i] = report[i];

            kbd_report_queue.head = (kbd_report_queue.head + 1) % HID_QUEUE_SIZE;
        }
        xhci_handle_repeat();
        sys_yield();
    }
}

void usb_mouse_task(void) {
    vga_print("[USB] Mouse Task Active.\n");
    while(1) {
        while(mouse_report_queue.head != mouse_report_queue.tail) {
            hid_report_t* report_obj = &mouse_report_queue.reports[mouse_report_queue.head];
            uint8_t* report = report_obj->data;
            int slot = report_obj->slot;

            mouse_state_t* ms = get_mouse_state();
            if (ms) {
                ms->left_button = report[0] & 0x01;
                ms->right_button = report[0] & 0x02;
                ms->middle_button = report[0] & 0x04;

                if (usb_devices[slot].is_tablet) {
                    uint16_t abs_x = (report[1] | (report[2] << 8));
                    uint16_t abs_y = (report[3] | (report[4] << 8));
                    ms->x = (abs_x * 639) / 32767;
                    ms->y = (abs_y * 479) / 32767;
                } else {
                    ms->x += (int8_t)report[1];
                    ms->y += (int8_t)report[2];
                    if (ms->x < 0) ms->x = 0;
                    if (ms->y < 0) ms->y = 0;
                    if (ms->x >= 640) ms->x = 639;
                    if (ms->y >= 480) ms->y = 479;
                }

                vga_draw_mouse(ms->x, ms->y);
                ms->active = true;
            }

            mouse_report_queue.head = (mouse_report_queue.head + 1) % HID_QUEUE_SIZE;
        }
        sys_yield();
    }
}

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
        xhci_handle_repeat();

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

            /* Wait for BIOS to release ownership (bit 16 drops to 0) */
            int timeout = 1000;
            while ((*ext_cap & (1 << 16)) && timeout--) {
                for(volatile int i=0; i<10000; i++);
            }
            if (timeout <= 0) {
                serial_write_str("[XHCI] BIOS Handover Timeout. Forcing...\n");
                *ext_cap &= ~(1 << 16);
            }

            /* legsup_ctl: Disable BIOS SMI generation */
            volatile uint32_t* legsup_ctl = ext_cap + 1;
            *legsup_ctl = 0; /* Clear all, especially SMI bits */
            break;
        }
        uint32_t next = (*ext_cap >> 8) & 0xFF;
        if (next == 0) break;
        ext_cap += next;
    }
}

int xhci_msc_read(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    int slot = (int)(uint64_t)priv;
    int ep_out = usb_devices[slot].bulk_out_idx;
    int ep_in = usb_devices[slot].bulk_in_idx;

    if (ep_out == 0 || ep_in == 0) return -1;

    usb_msc_cbw_t cbw = {0};
    cbw.dCBWSignature = MSC_CBW_SIGNATURE;
    cbw.dCBWTag = 0xDEADBEEF;
    cbw.dCBWDataTransferLength = count * 512;
    cbw.bmCBWFlags = 0x80; /* Data In */
    cbw.bCBWLUN = 0;
    cbw.bCBWCBLength = 10;

    /* SCSI READ(10) */
    cbw.CBWCB[0] = 0x28;
    cbw.CBWCB[2] = (lba >> 24) & 0xFF;
    cbw.CBWCB[3] = (lba >> 16) & 0xFF;
    cbw.CBWCB[4] = (lba >> 8) & 0xFF;
    cbw.CBWCB[5] = lba & 0xFF;
    cbw.CBWCB[7] = (count >> 8) & 0xFF;
    cbw.CBWCB[8] = count & 0xFF;

    /* 1. Send CBW */
    xhci_trb_t trb = {0};
    trb.ptr = vmm_get_phys(&cbw);
    trb.status = sizeof(cbw);
    trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC */
    xhci_transfer(slot, ep_out, &trb);
    if (xhci_wait_transfer(slot, ep_out) != 0) return -1;

    /* 2. Read Data */
    trb.ptr = vmm_get_phys(buffer);
    trb.status = count * 512;
    trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC */
    xhci_transfer(slot, ep_in, &trb);
    if (xhci_wait_transfer(slot, ep_in) != 0) return -1;

    /* 3. Read CSW */
    usb_msc_csw_t csw = {0};
    trb.ptr = vmm_get_phys(&csw);
    trb.status = sizeof(csw);
    trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC */
    xhci_transfer(slot, ep_in, &trb);
    if (xhci_wait_transfer(slot, ep_in) != 0) return -1;

    if (csw.dCSWSignature != MSC_CSW_SIGNATURE || csw.bCSWStatus != 0) {
        serial_print("[USB] MSC Read Failure: Sig=0x%x Status=0x%x\n", csw.dCSWSignature, csw.bCSWStatus);
        return -1;
    }
    return 0;
}

int xhci_msc_write(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    int slot = (int)(uint64_t)priv;
    int ep_out = usb_devices[slot].bulk_out_idx;
    int ep_in = usb_devices[slot].bulk_in_idx;

    if (ep_out == 0 || ep_in == 0) return -1;

    usb_msc_cbw_t cbw = {0};
    cbw.dCBWSignature = MSC_CBW_SIGNATURE;
    cbw.dCBWTag = 0x87654321;
    cbw.dCBWDataTransferLength = count * 512;
    cbw.bmCBWFlags = 0x00; /* Data Out */
    cbw.bCBWLUN = 0;
    cbw.bCBWCBLength = 10;

    /* SCSI WRITE(10) */
    cbw.CBWCB[0] = 0x2A;
    cbw.CBWCB[2] = (lba >> 24) & 0xFF;
    cbw.CBWCB[3] = (lba >> 16) & 0xFF;
    cbw.CBWCB[4] = (lba >> 8) & 0xFF;
    cbw.CBWCB[5] = lba & 0xFF;
    cbw.CBWCB[7] = (count >> 8) & 0xFF;
    cbw.CBWCB[8] = count & 0xFF;

    /* 1. Send CBW */
    xhci_trb_t trb = {0};
    trb.ptr = vmm_get_phys(&cbw);
    trb.status = sizeof(cbw);
    trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC */
    xhci_transfer(slot, ep_out, &trb);
    if (xhci_wait_transfer(slot, ep_out) != 0) return -1;

    /* 2. Write Data */
    trb.ptr = vmm_get_phys(buffer);
    trb.status = count * 512;
    trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC */
    xhci_transfer(slot, ep_out, &trb);
    if (xhci_wait_transfer(slot, ep_out) != 0) return -1;

    /* 3. Read CSW */
    usb_msc_csw_t csw = {0};
    trb.ptr = vmm_get_phys(&csw);
    trb.status = sizeof(csw);
    trb.control = (TRB_TYPE_NORMAL << 10) | (1 << 5); /* IOC */
    xhci_transfer(slot, ep_in, &trb);
    if (xhci_wait_transfer(slot, ep_in) != 0) return -1;

    if (csw.dCSWSignature != MSC_CSW_SIGNATURE || csw.bCSWStatus != 0) {
        serial_print("[USB] MSC Write Failure: Sig=0x%x Status=0x%x\n", csw.dCSWSignature, csw.bCSWStatus);
        return -1;
    }
    return 0;
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
