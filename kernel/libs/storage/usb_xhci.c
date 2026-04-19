#include <kernel/libs/core/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <kernel/libs/storage/xhci.h>

void* malloc(size_t size);
void free(void* ptr);
void* slab_alloc_aligned(int id, size_t size, size_t align);
void* pmm_alloc(uint64_t count);
uint64_t vmm_get_phys(void* virt);
uint64_t get_hhdm_offset(void);
void pit_wait_ms(uint32_t ms);
void serial_write_str(const char* s);
void vga_print(const char* fmt, ...);
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void console_push_char(char c);

static xhci_controller_t* g_xhci_controllers[4];
static int g_xhci_count = 0;

/* --- Local Helpers --- */

static void memset(void* ptr, int val, size_t size) {
    uint8_t* p = (uint8_t*)ptr;
    while (size--) *p++ = (uint8_t)val;
}

static void memcpy(void* dest, const void* src, size_t size) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (size--) *d++ = *s++;
}

void* get_xhci_base(void) {
    if (g_xhci_count > 0) return (void*)g_xhci_controllers[0]->xhciVirtualAddress;
    return NULL;
}

/* --- XHCI Ring Management --- */

void xhci_on_interrupt(xhci_controller_t* hcd);

void xhci_command_ring_init(xhci_controller_t* hcd, xhci_command_ring_t* ring) {
    ring->hcd = hcd;
    void* ptr = pmm_alloc(1);
    ring->physicalAddr = (uintptr_t)ptr;
    ring->ring = (xhci_trb_t*)((uint64_t)ptr + get_hhdm_offset());
    memset(ring->ring, 0, 4096);
    ring->enqueueIndex = 0;
    ring->maxIndex = 4096 / XHCI_TRB_SIZE - 1;
    xhci_link_trb_t* link = (xhci_link_trb_t*)&ring->ring[ring->maxIndex];
    link->trbType = TRBTypeLink;
    link->cycleBit = 1;
    link->segmentPtr = ring->physicalAddr;
    link->toggleCycle = 1;
    ring->cycleState = true;
    ring->events = (xhci_command_completion_event_t*)malloc(sizeof(xhci_command_completion_event_t) * ring->maxIndex);
    memset(ring->events, 0, sizeof(xhci_command_completion_event_t) * ring->maxIndex);
}

xhci_command_completion_event_trb_t xhci_command_send(xhci_command_ring_t* ring, void* data) {
    xhci_trb_t* command = (xhci_trb_t*)data;
    xhci_command_completion_event_t* ev = &ring->events[ring->enqueueIndex];
    ev->completed = false;
    command->cycleBit = ring->cycleState ? 1 : 0;
    ring->ring[ring->enqueueIndex] = *command;
    ring->enqueueIndex++;
    if (ring->enqueueIndex >= ring->maxIndex) {
        ring->enqueueIndex = 0;
        ring->cycleState = !ring->cycleState;
    }
    ring->hcd->doorbellRegs[0].doorbell = 0;
    int timeout = 1000000;
    while (!ev->completed && timeout--) {
        xhci_on_interrupt(ring->hcd);
        __asm__ volatile("pause");
    }
    return *((xhci_command_completion_event_trb_t*)&ev->event);
}

void xhci_transfer_ring_init(xhci_transfer_ring_t* ring) {
    void* ptr = pmm_alloc(1);
    ring->physicalAddr = (uintptr_t)ptr;
    ring->ring = (xhci_trb_t*)((uint64_t)ptr + get_hhdm_offset());
    memset(ring->ring, 0, 4096);
    ring->enqueueIndex = 0;
    ring->maxIndex = 4096 / XHCI_TRB_SIZE - 1;
    xhci_link_trb_t* link = (xhci_link_trb_t*)&ring->ring[ring->maxIndex];
    link->trbType = TRBTypeLink;
    link->cycleBit = 1;
    link->segmentPtr = ring->physicalAddr;
    link->toggleCycle = 1;
    ring->cycleState = true;
    ring->last_transfer_status = -1;
}

void xhci_transfer(xhci_controller_t* hcd, uint8_t slot_id, uint8_t dci, void* data, uint32_t len, bool in) {
    xhci_device_t* dev = hcd->devices[slot_id];
    xhci_transfer_ring_t* ring = &dev->rings[dci];
    ring->last_transfer_status = -1;
    xhci_setup_trb_t setup;
    memset(&setup, 0, sizeof(setup));
    setup.trbType = TRBTypeSetup;
    setup.immediateData = 1;
    setup.trbTransferLength = 8;
    setup.transferType = (len > 0) ? (in ? INDataStage : OUTDataStage) : NoDataStage;
    memcpy(&setup, data, 8);
    setup.cycleBit = ring->cycleState ? 1 : 0;
    ring->ring[ring->enqueueIndex++] = *((xhci_trb_t*)&setup);
    if (ring->enqueueIndex >= ring->maxIndex) { ring->enqueueIndex = 0; ring->cycleState = !ring->cycleState; }
    if (len > 0) {
        xhci_data_trb_t data_trb;
        memset(&data_trb, 0, sizeof(data_trb));
        data_trb.trbType = TRBTypeData;
        data_trb.dataBuffer = vmm_get_phys((uint8_t*)data + 8);
        data_trb.transferLength = len;
        data_trb.direction = in ? 1 : 0;
        data_trb.interruptOnCompletion = 1;
        data_trb.cycleBit = ring->cycleState ? 1 : 0;
        ring->ring[ring->enqueueIndex++] = *((xhci_trb_t*)&data_trb);
        if (ring->enqueueIndex >= ring->maxIndex) { ring->enqueueIndex = 0; ring->cycleState = !ring->cycleState; }
    }
    xhci_status_trb_t status_trb;
    memset(&status_trb, 0, sizeof(status_trb));
    status_trb.trbType = TRBTypeStatus;
    status_trb.direction = in ? 0 : 1;
    status_trb.interruptOnCompletion = 1;
    status_trb.cycleBit = ring->cycleState ? 1 : 0;
    ring->ring[ring->enqueueIndex++] = *((xhci_trb_t*)&status_trb);
    if (ring->enqueueIndex >= ring->maxIndex) { ring->enqueueIndex = 0; ring->cycleState = !ring->cycleState; }
    hcd->doorbellRegs[slot_id].target = dci;
    int timeout = 1000000;
    while (ring->last_transfer_status == -1 && timeout--) {
        xhci_on_interrupt(hcd);
        __asm__ volatile("pause");
    }
}

void xhci_event_ring_init(xhci_controller_t* hcd, xhci_event_ring_t* ring) {
    ring->hcd = hcd;
    void* seg_table_phys = pmm_alloc(1);
    ring->segmentsPhys = (uintptr_t)seg_table_phys;
    xhci_event_ring_segment_table_entry_t* table = (xhci_event_ring_segment_table_entry_t*)((uint64_t)seg_table_phys + get_hhdm_offset());
    ring->segmentTable = table;
    void* seg_phys = pmm_alloc(1);
    ring->segments = (xhci_event_ring_segment_t*)malloc(sizeof(xhci_event_ring_segment_t));
    ring->segments[0].physicalAddr = (uintptr_t)seg_phys;
    ring->segments[0].segment = (xhci_event_trb_t*)((uint64_t)seg_phys + get_hhdm_offset());
    ring->segments[0].size = 4096 / XHCI_TRB_SIZE;
    memset(ring->segments[0].segment, 0, 4096);
    table[0].ringSegmentBaseAddress = ring->segments[0].physicalAddr;
    table[0].ringSegmentSize = ring->segments[0].size;
    ring->dequeueIndex = 0;
    ring->cycleState = true;
}

/* --- HID Keyboard Driver --- */

static char usb_map[256] = {
    [0x04]='a',[0x05]='b',[0x06]='c',[0x07]='d',[0x08]='e',[0x09]='f',[0x0A]='g',[0x0B]='h',[0x0C]='i',
    [0x0D]='j',[0x0E]='k',[0x0F]='l',[0x10]='m',[0x11]='n',[0x12]='o',[0x13]='p',[0x14]='q',[0x15]='r',
    [0x16]='s',[0x17]='t',[0x18]='u',[0x19]='v', [0x1A]='w',[0x1B]='x',[0x1C]='y',[0x1D]='z',
    [0x1E]='1',[0x1F]='2',[0x20]='3',[0x21]='4',[0x22]='5',[0x23]='6',[0x24]='7',[0x25]='8',[0x26]='9',[0x27]='0',
    [0x28]='\n',[0x29]=27,[0x2A]='\b',[0x2B]='\t',[0x2C]=' ',[0x2D]='-',[0x2E]='=',[0x2F]='[',[0x30]=']',[0x31]='\\',
    [0x33]=';',[0x34]='\'',[0x35]='`',[0x36]=',',[0x37]='.',[0x38]='/'
};

static char usb_shift_map[256] = {
    [0x04]='A',[0x05]='B',[0x06]='C',[0x07]='D',[0x08]='E',[0x09]='F',[0x0A]='G',[0x0B]='H',[0x0C]='I',
    [0x0D]='J',[0x0E]='K',[0x0F]='L',[0x10]='M',[0x11]='N',[0x12]='O',[0x13]='P',[0x14]='Q',[0x15]='R',
    [0x16]='S',[0x17]='T',[0x18]='U',[0x19]='V', [0x1A]='W',[0x1B]='X',[0x1C]='Y',[0x1D]='Z',
    [0x1E]='!',[0x1F]='@',[0x20]='#',[0x21]='$',[0x22]='%',[0x23]='^',[0x24]='&',[0x25]='*',[0x26]='(',[0x27]=')',
    [0x2D]='_',[0x2E]='+',[0x2F]='{',[0x30]='}',[0x31]='|',[0x33]=':',[0x34]='"',[0x35]='~',[0x36]='<',[0x37]='>',[0x38]='?'
};

void usb_keyboard_handle(uint8_t* report) {
    uint8_t modifiers = report[0];
    bool shift = (modifiers & 0x02) || (modifiers & 0x20);
    for (int i = 2; i < 8; i++) {
        if (report[i] != 0) {
            char c = shift ? usb_shift_map[report[i]] : usb_map[report[i]];
            if (c) console_push_char(c);
        }
    }
}

/* --- Device Enumeration --- */

void xhci_setup_device(xhci_controller_t* hcd, uint8_t port_id) {
    xhci_enable_slot_command_trb_t en_slot;
    memset(&en_slot, 0, sizeof(en_slot));
    en_slot.trbType = TRBTypeEnableSlotCommand;
    xhci_command_completion_event_trb_t resp = xhci_command_send(&hcd->commandRing, &en_slot);
    uint8_t slot_id = resp.slotID;
    xhci_device_t* dev = (xhci_device_t*)malloc(sizeof(xhci_device_t));
    memset(dev, 0, sizeof(xhci_device_t));
    dev->slot_id = slot_id;
    hcd->devices[slot_id] = dev;
    void* ctx_phys = pmm_alloc(1);
    dev->device_context_phys = (uintptr_t)ctx_phys;
    dev->device_context = (xhci_device_context_t*)((uint64_t)ctx_phys + get_hhdm_offset());
    memset(dev->device_context, 0, 4096);
    hcd->devContextBaseAddressArray[slot_id] = dev->device_context_phys;
    xhci_transfer_ring_init(&dev->rings[1]); // EP0 DCI = 1
    void* input_phys = pmm_alloc(1);
    xhci_input_context_t* input = (xhci_input_context_t*)((uint64_t)input_phys + get_hhdm_offset());
    memset(input, 0, 4096);
    input->control.add_flags = 3;
    input->device.slot.rootHubPortNumber = port_id;
    input->device.slot.ctxEntries = 1;
    input->device.controlEndpoint.endpointType = EndpointTypeControl;
    input->device.controlEndpoint.maxPacketSize = 64;
    input->device.controlEndpoint.trDequeuePointer = (dev->rings[1].physicalAddr >> 4);
    input->device.controlEndpoint.dequeueCycleState = 1;
    input->device.controlEndpoint.errorCount = 3;
    xhci_address_device_command_trb_t addr_cmd;
    memset(&addr_cmd, 0, sizeof(addr_cmd));
    addr_cmd.trbType = TRBTypeAddressDeviceCommand;
    addr_cmd.slotID = slot_id;
    addr_cmd.inputContextPointer = (uintptr_t)input_phys;
    xhci_command_send(&hcd->commandRing, &addr_cmd);

    /* Fetch Device Descriptor */
    uint8_t setup_pkt[8] = { 0x80, USB_REQ_GET_DESCRIPTOR, 0, USB_DESC_DEVICE, 0, 0, 18, 0 };
    uint8_t* desc_buf = malloc(64);
    memcpy(desc_buf, setup_pkt, 8);
    xhci_transfer(hcd, slot_id, 1, desc_buf, 18, true);
    usb_device_descriptor_t* dev_desc = (usb_device_descriptor_t*)(desc_buf + 8);
    if (dev_desc->bDeviceClass == 0x00) {
        uint8_t setup_cfg[8] = { 0x80, USB_REQ_GET_DESCRIPTOR, 0, USB_DESC_CONFIG, 0, 0, 9, 0 };
        memcpy(desc_buf, setup_cfg, 8);
        xhci_transfer(hcd, slot_id, 1, desc_buf, 9, true);
        usb_config_descriptor_t* cfg = (usb_config_descriptor_t*)(desc_buf + 8);
        uint16_t total = cfg->wTotalLength;
        memcpy(desc_buf, setup_cfg, 8);
        desc_buf[6] = (uint8_t)total; desc_buf[7] = (uint8_t)(total >> 8);
        xhci_transfer(hcd, slot_id, 1, desc_buf, total, true);
    }
    serial_write_str("[XHCI] Device addressed.\n");
}

/* --- Interrupt & Tasking --- */

void xhci_on_interrupt(xhci_controller_t* hcd) {
    if (!(hcd->opRegs->usbStatus & USB_STS_EINT)) return;
    hcd->interrupter->interruptPending = 1;
    hcd->opRegs->usbStatus |= USB_STS_EINT;
    while (1) {
        xhci_event_trb_t* e = &hcd->eventRing.segments[0].segment[hcd->eventRing.dequeueIndex];
        if (!!e->cycleBit != hcd->eventRing.cycleState) break;
        xhci_event_trb_t ev = *e;
        if (ev.trbType == TRBTypeCommandCompletionEvent) {
            xhci_command_completion_event_trb_t* completion = (xhci_command_completion_event_trb_t*)&ev;
            uint32_t index = (completion->commandTRBPointer - hcd->commandRing.physicalAddr) / sizeof(xhci_trb_t);
            if (index < hcd->commandRing.maxIndex) {
                hcd->commandRing.events[index].event = *((xhci_trb_t*)completion);
                hcd->commandRing.events[index].completed = true;
            }
        } else if (ev.trbType == TRBTypeTransferEvent) {
            xhci_transfer_event_trb_t* te = (xhci_transfer_event_trb_t*)&ev;
            xhci_device_t* dev = hcd->devices[te->slotID];
            if (dev) {
                /* DCI calculation: endpointID is 1-31. Control EP is 1. */
                dev->rings[te->endpointID].last_transfer_status = te->completionCode;
                if (te->pointer) {
                    uint8_t* data = (uint8_t*)(te->pointer + get_hhdm_offset());
                    if (te->endpointID == 3) usb_keyboard_handle(data);
                }
            }
        }
        hcd->eventRing.dequeueIndex++;
        if (hcd->eventRing.dequeueIndex >= hcd->eventRing.segments[0].size) {
            hcd->eventRing.dequeueIndex = 0;
            hcd->eventRing.cycleState = !hcd->eventRing.cycleState;
        }
    }
    hcd->interrupter->eventRingDequeuePointer =
        (hcd->eventRing.segments[0].physicalAddr + hcd->eventRing.dequeueIndex * XHCI_TRB_SIZE) | XHCI_INT_ERDP_BUSY;
    hcd->interrupter->interruptPending = 0;
}

void xhci_irq_handler(void) {
    for (int i = 0; i < g_xhci_count; i++) xhci_on_interrupt(g_xhci_controllers[i]);
}

void usb_sovereign_task(void) {
    serial_write_str("[USB] Sovereign Task Started.\n");
    while (1) {
        xhci_irq_handler();
        __asm__ volatile ("int $0x81");
    }
}

/* --- Initialization --- */

bool xhci_take_ownership(xhci_controller_t* hcd) {
    xhci_ext_cap_legacy_support_t* cap = (xhci_ext_cap_legacy_support_t*)hcd->extCapabilities;
    if (cap->capID != 1) return true;
    if (cap->controllerBIOSSemaphore) {
        serial_write_str("[XHCI] Requesting Handover...\n");
        cap->controllerOSSemaphore = 1;
    }
    int timer = 20;
    while (cap->controllerBIOSSemaphore && timer--) pit_wait_ms(1);
    return !cap->controllerBIOSSemaphore;
}

void xhci_controller_init(xhci_controller_t* hcd, uintptr_t base) {
    hcd->xhciBaseAddress = base;
    hcd->xhciVirtualAddress = base + get_hhdm_offset();
    hcd->capRegs = (xhci_cap_regs_t*)hcd->xhciVirtualAddress;
    hcd->opRegs = (xhci_op_regs_t*)(hcd->xhciVirtualAddress + hcd->capRegs->capLength);
    hcd->portRegs = (xhci_port_regs_t*)(hcd->xhciVirtualAddress + hcd->capRegs->capLength + 0x400);
    hcd->runtimeRegs = (xhci_runtime_regs_t*)(hcd->xhciVirtualAddress + (hcd->capRegs->rtsOff & ~0x1F));
    hcd->doorbellRegs = (xhci_doorbell_register_t*)(hcd->xhciVirtualAddress + (hcd->capRegs->dbOff & ~0x3));
    uint32_t xecp = (hcd->capRegs->hccParams1 >> 16) & 0xFFFF;
    if (xecp) {
        hcd->extCapabilities = (uintptr_t)hcd->capRegs + (xecp << 2);
        xhci_take_ownership(hcd);
    }
    hcd->opRegs->usbCommand &= ~USB_CMD_RS;
    while (!(hcd->opRegs->usbStatus & USB_STS_HCH)) pit_wait_ms(1);
    hcd->opRegs->usbCommand |= USB_CMD_HCRST;
    while (hcd->opRegs->usbStatus & USB_STS_CNR) pit_wait_ms(1);
    void* dcbaa_phys = pmm_alloc(1);
    hcd->devContextBaseAddressArray = (uint64_t*)((uint64_t)dcbaa_phys + get_hhdm_offset());
    memset(hcd->devContextBaseAddressArray, 0, 4096);
    hcd->opRegs->devContextBaseAddrArrayPtr = (uintptr_t)dcbaa_phys;
    xhci_command_ring_init(hcd, &hcd->commandRing);
    hcd->opRegs->cmdRingCtl = hcd->commandRing.physicalAddr | 1;
    hcd->opRegs->configure = (hcd->opRegs->configure & ~0xFF) | (hcd->capRegs->hcsParams1 & 0xFF);
    xhci_event_ring_init(hcd, &hcd->eventRing);
    hcd->interrupter = &hcd->runtimeRegs->interrupters[0];
    hcd->interrupter->eventRingSegmentTableSize = 1;
    hcd->interrupter->eventRingSegmentTableBaseAddress = hcd->eventRing.segmentsPhys;
    hcd->interrupter->eventRingDequeuePointer = hcd->eventRing.segments[0].physicalAddr | XHCI_INT_ERDP_BUSY;
    hcd->interrupter->interruptEnable = 1;
    hcd->opRegs->usbStatus |= USB_STS_EINT;
    hcd->opRegs->usbCommand |= USB_CMD_HSEE | USB_CMD_INTE | USB_CMD_RS;
    while (hcd->opRegs->usbStatus & USB_STS_HCH) pit_wait_ms(1);
    hcd->controllerStatus = ControllerInitialized;
}

void usb_xhci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                    uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                    uint8_t base_class = (class_info >> 24) & 0xFF;
                    uint8_t sub_class = (class_info >> 16) & 0xFF;
                    uint8_t prog_if = (class_info >> 8) & 0xFF;
                    if (base_class == 0x0C && sub_class == 0x03 && prog_if == 0x30) {
                        pci_enable_master(bus, slot, func);
                        uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                        uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
                        uintptr_t base = (uintptr_t)(bar0 & 0xFFFFFFF0);
                        if ((bar0 & 0x06) == 0x04) base |= ((uintptr_t)bar1 << 32);
                        xhci_controller_t* hcd = malloc(sizeof(xhci_controller_t));
                        memset(hcd, 0, sizeof(xhci_controller_t));
                        xhci_controller_init(hcd, base);
                        if (hcd->controllerStatus == ControllerInitialized) {
                            g_xhci_controllers[g_xhci_count++] = hcd;
                            for (int i = 0; i < (int)(hcd->capRegs->hcsParams1 >> 24); i++) {
                                if (hcd->portRegs[i].portSC & 1) xhci_setup_device(hcd, i + 1);
                            }
                        }
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
        if (g_xhci_count > 0) idt_set_descriptor(43, xhci_irq_handler, 0x8E);
    }
}
