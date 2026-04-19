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

void xhci_command_ring_init(xhci_controller_t* hcd, xhci_command_ring_t* ring) {
    ring->hcd = hcd;
    void* ptr = pmm_alloc(1);
    ring->physicalAddr = (uintptr_t)ptr;
    ring->ring = (xhci_trb_t*)((uint64_t)ptr + get_hhdm_offset());

    memset(ring->ring, 0, 4096);

    ring->enqueueIndex = 0;
    ring->maxIndex = 4096 / XHCI_TRB_SIZE;
    ring->maxIndex--; /* Last one is Link TRB */

    xhci_link_trb_t* link = (xhci_link_trb_t*)&ring->ring[ring->maxIndex];
    memset(link, 0, sizeof(xhci_link_trb_t));
    link->trbType = TRBTypeLink;
    link->cycleBit = 1;
    link->segmentPtr = ring->physicalAddr;
    link->toggleCycle = 1;

    ring->cycleState = true;
    ring->events = (xhci_command_completion_event_t*)malloc(sizeof(xhci_command_completion_event_t) * ring->maxIndex);
    memset(ring->events, 0, sizeof(xhci_command_completion_event_t) * ring->maxIndex);
}

void xhci_command_send_raw(xhci_command_ring_t* ring, void* data) {
    xhci_trb_t* command = (xhci_trb_t*)data;

    if (ring->cycleState)
        command->cycleBit = 1;
    else
        command->cycleBit = 0;

    memcpy(&ring->ring[ring->enqueueIndex], command, sizeof(xhci_trb_t));
    ring->enqueueIndex++;

    if (ring->enqueueIndex >= ring->maxIndex) {
        ring->enqueueIndex = 0;
        ring->cycleState = !ring->cycleState;
    }
}

xhci_command_completion_event_trb_t xhci_command_send(xhci_command_ring_t* ring, void* data) {
    xhci_trb_t* command = (xhci_trb_t*)data;
    xhci_command_completion_event_t* ev = &ring->events[ring->enqueueIndex];
    ev->completed = false;

    if (ring->cycleState)
        command->cycleBit = 1;
    else
        command->cycleBit = 0;

    ring->ring[ring->enqueueIndex] = *command;
    ring->enqueueIndex++;

    if (ring->enqueueIndex >= ring->maxIndex) {
        ring->enqueueIndex = 0;
        ring->cycleState = !ring->cycleState;
    }

    ring->hcd->doorbellRegs[0].doorbell = 0;

    /* Busy wait for completion */
    int timeout = 1000000;
    while (!ev->completed && timeout--) {
        __asm__ volatile("pause");
    }

    return ev->event;
}

void xhci_event_ring_init(xhci_controller_t* hcd, xhci_event_ring_t* ring) {
    ring->hcd = hcd;
    ring->segmentCount = 1;

    void* seg_table_phys = pmm_alloc(1);
    ring->segmentsPhys = (uintptr_t)seg_table_phys;
    ring->segmentTable = (xhci_event_ring_segment_table_entry_t*)((uint64_t)seg_table_phys + get_hhdm_offset());
    memset(ring->segmentTable, 0, 4096);

    ring->segments = (xhci_event_ring_segment_t*)malloc(sizeof(xhci_event_ring_segment_t) * ring->segmentCount);

    for (uint32_t i = 0; i < ring->segmentCount; i++) {
        void* seg_phys = pmm_alloc(1);
        ring->segments[i].physicalAddr = (uintptr_t)seg_phys;
        ring->segments[i].segment = (xhci_event_trb_t*)((uint64_t)seg_phys + get_hhdm_offset());
        ring->segments[i].size = 4096 / XHCI_TRB_SIZE;

        memset(ring->segments[i].segment, 0, 4096);

        ring->segmentTable[i].ringSegmentBaseAddress = ring->segments[i].physicalAddr;
        ring->segmentTable[i].ringSegmentSize = ring->segments[i].size;
    }

    ring->dequeueIndex = 0;
    ring->cycleState = true;
}

bool xhci_event_dequeue(xhci_event_ring_t* ring, xhci_event_trb_t* outTRB) {
    xhci_event_trb_t* ev = &ring->segments[0].segment[ring->dequeueIndex];
    if (!!ev->cycleBit == !!ring->cycleState) {
        *outTRB = *ev;

        ring->dequeueIndex++;
        if (ring->dequeueIndex >= ring->segments[0].size) {
            ring->dequeueIndex = 0;
            ring->cycleState = !ring->cycleState;
        }
        return true;
    }
    return false;
}

/* --- Controller Ownership & Initialization --- */

bool xhci_take_ownership(xhci_controller_t* hcd) {
    xhci_ext_cap_legacy_support_t* cap = (xhci_ext_cap_legacy_support_t*)hcd->extCapabilities;

    if (cap->capID != XHCIExtCapLegacySupport) {
        return true;
    }

    if (cap->controllerBIOSSemaphore) {
        serial_write_str("[XHCI] BIOS owns host controller, attempting to take ownership...\n");
        cap->controllerOSSemaphore = 1;
    }

    int timer = 20;
    while (cap->controllerBIOSSemaphore && timer--) {
        pit_wait_ms(1);
    }

    if (cap->controllerBIOSSemaphore) {
        return false;
    }

    return true;
}

void xhci_on_interrupt(xhci_controller_t* hcd) {
    hcd->interrupter->interruptPending = 1;

    if (!(hcd->opRegs->usbStatus & USB_STS_EINT)) {
        return;
    }

    hcd->opRegs->usbStatus |= USB_STS_EINT;

    xhci_event_trb_t ev;
    while (xhci_event_dequeue(&hcd->eventRing, &ev)) {
        if (ev.trbType == TRBTypeCommandCompletionEvent) {
            xhci_command_completion_event_trb_t* completion = (xhci_command_completion_event_trb_t*)&ev;
            uint64_t ring_base = hcd->commandRing.physicalAddr;
            uint32_t index = (completion->commandTRBPointer - ring_base) / sizeof(xhci_trb_t);
            if (index < hcd->commandRing.maxIndex) {
                hcd->commandRing.events[index].event = *completion;
                hcd->commandRing.events[index].completed = true;
            }
        }
    }

    hcd->interrupter->eventRingDequeuePointer =
        (hcd->eventRing.segments[0].physicalAddr + hcd->eventRing.dequeueIndex * XHCI_TRB_SIZE) |
        XHCI_INT_ERDP_BUSY;
    hcd->interrupter->interruptPending = 0;
    hcd->opRegs->usbStatus &= ~USB_STS_EINT;
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
    if (xecp != 0) {
        hcd->extCapabilities = (uintptr_t)hcd->capRegs + (xecp << 2);
        if (!xhci_take_ownership(hcd)) {
            serial_write_str("[XHCI] Failed to take controller ownership!\n");
            return;
        }
    }

    hcd->opRegs->usbCommand &= ~USB_CMD_RS;
    int timer = 20;
    while (timer-- && !(hcd->opRegs->usbStatus & USB_STS_HCH))
        pit_wait_ms(1);

    if (!(hcd->opRegs->usbStatus & USB_STS_HCH)) {
        serial_write_str("[XHCI] Controller not halted\n");
        return;
    }

    hcd->opRegs->usbCommand |= USB_CMD_HCRST;
    pit_wait_ms(10);
    timer = 20;
    while (timer-- && (hcd->opRegs->usbStatus & USB_STS_CNR))
        pit_wait_ms(1);

    if (hcd->opRegs->usbStatus & USB_STS_CNR) {
        serial_write_str("[XHCI] Controller Timed Out\n");
        return;
    }

    /* DCBAA */
    void* dcbaa_phys = pmm_alloc(1);
    hcd->devContextBaseAddressArrayPhys = (uintptr_t)dcbaa_phys;
    hcd->devContextBaseAddressArray = (uint64_t*)((uint64_t)dcbaa_phys + get_hhdm_offset());
    memset(hcd->devContextBaseAddressArray, 0, 4096);
    hcd->opRegs->devContextBaseAddrArrayPtr = hcd->devContextBaseAddressArrayPhys;

    /* Scratchpad Buffers */
    uint16_t max_scratchpad = (uint16_t)(((hcd->capRegs->hcsParams2 >> 21) & 0x1F) << 5) | ((hcd->capRegs->hcsParams2 >> 27) & 0x1F);
    if (max_scratchpad > 0) {
        void* scratch_array_phys = pmm_alloc(1);
        hcd->scratchpadBuffersPhys = (uintptr_t)scratch_array_phys;
        hcd->scratchpadBuffers = (uint64_t*)((uint64_t)scratch_array_phys + get_hhdm_offset());
        memset(hcd->scratchpadBuffers, 0, 4096);

        for (int i = 0; i < max_scratchpad && i < 512; i++) {
            hcd->scratchpadBuffers[i] = (uint64_t)pmm_alloc(1);
        }
        hcd->devContextBaseAddressArray[0] = hcd->scratchpadBuffersPhys;
    }

    xhci_command_ring_init(hcd, &hcd->commandRing);
    hcd->opRegs->cmdRingCtl = hcd->commandRing.physicalAddr | 1;

    hcd->maxSlots = 40;
    uint8_t hw_max_slots = hcd->capRegs->hcsParams1 & 0xFF;
    if (hcd->maxSlots > hw_max_slots) hcd->maxSlots = hw_max_slots;
    hcd->opRegs->configure = (hcd->opRegs->configure & ~0xFF) | hcd->maxSlots;

    xhci_event_ring_init(hcd, &hcd->eventRing);

    hcd->interrupter = &hcd->runtimeRegs->interrupters[0];
    hcd->interrupter->interruptEnable = 1;
    hcd->interrupter->interruptPending = 1;
    hcd->interrupter->eventRingSegmentTableSize = hcd->eventRing.segmentCount;
    hcd->interrupter->eventRingSegmentTableBaseAddress = hcd->eventRing.segmentsPhys;
    hcd->interrupter->eventRingDequeuePointer = hcd->eventRing.segments[0].physicalAddr | XHCI_INT_ERDP_BUSY;

    hcd->opRegs->usbStatus |= USB_STS_EINT;
    hcd->opRegs->usbCommand |= USB_CMD_HSEE | USB_CMD_INTE | USB_CMD_RS;

    timer = 20;
    while (timer-- && (hcd->opRegs->usbStatus & USB_STS_HCH))
        pit_wait_ms(1);

    if (hcd->opRegs->usbStatus & USB_STS_HCH) {
        serial_write_str("[XHCI] Controller Halted\n");
        return;
    }

    hcd->controllerStatus = ControllerInitialized;
}

/* --- Port & Protocol Discovery --- */

void xhci_initialize_protocols(xhci_controller_t* hcd) {
    hcd->protocolCapacity = 8;
    hcd->protocols = (xhci_ext_cap_supported_protocol_t**)malloc(sizeof(xhci_ext_cap_supported_protocol_t*) * hcd->protocolCapacity);
    hcd->protocolCount = 0;

    if (hcd->extCapabilities == 0) return;

    xhci_ext_cap_supported_protocol_t* cap = (xhci_ext_cap_supported_protocol_t*)hcd->extCapabilities;
    while (cap) {
        if (cap->capID == XHCIExtCapSupportedProtocol) {
            if (hcd->protocolCount < hcd->protocolCapacity) {
                hcd->protocols[hcd->protocolCount++] = cap;
                serial_write_str("[XHCI] Found Protocol: ");
                char name[5]; memcpy(name, (void*)cap->name, 4); name[4] = 0;
                serial_write_str(name);
                serial_write_str("\n");
            }
        }

        if (cap->nextCap == 0) break;
        cap = (xhci_ext_cap_supported_protocol_t*)((uintptr_t)cap + (cap->nextCap << 2));
    }
}

void xhci_initialize_ports(xhci_controller_t* hcd) {
    uint8_t max_ports = (hcd->capRegs->hcsParams1 >> 24) & 0xFF;
    hcd->ports = (xhci_port_t*)malloc(sizeof(xhci_port_t) * max_ports);

    for (uint32_t p = 0; p < hcd->protocolCount; p++) {
        xhci_ext_cap_supported_protocol_t* proto = hcd->protocols[p];
        for (uint32_t i = proto->portOffset; i < proto->portOffset + proto->portCount && i <= max_ports; i++) {
            xhci_port_regs_t* port = &hcd->portRegs[i - 1];
            hcd->ports[i - 1].protocol = proto;
            hcd->ports[i - 1].registers = port;

            if (!(port->portSC & XHCI_PORTSC_PP)) {
                port->portSC |= XHCI_PORTSC_PP;
                pit_wait_ms(20);
            }

            if (proto->majorRevision == 0x03) {
                port->portSC |= XHCI_PORTSC_WPR; /* Warm Reset */
            } else {
                port->portSC |= XHCI_PORTSC_PR; /* Reset */
            }

            int timeout = 25;
            while (timeout-- && !(port->portSC & XHCI_PORTSC_PRC)) {
                pit_wait_ms(1);
            }

            if (port->portSC & XHCI_PORTSC_PED) {
                serial_write_str("[XHCI] Port enabled!\n");
            }
        }
    }
}

void xhci_enable_slot(xhci_controller_t* hcd) {
    xhci_enable_slot_command_trb_t trb;
    memset(&trb, 0, XHCI_TRB_SIZE);
    trb.trbType = TRBTypeEnableSlotCommand;
    xhci_command_send(&hcd->commandRing, &trb);
}

/* --- Service Integration --- */

void usb_sovereign_task(void) {
    serial_write_str("[USB] Sovereign Task Started.\n");
    while (1) {
        for (int i = 0; i < g_xhci_count; i++) {
            xhci_on_interrupt(g_xhci_controllers[i]);
        }
        /* sys_yield() */
        __asm__ volatile ("int $0x81");
    }
}

void xhci_irq_handler(void) {
    for (int i = 0; i < g_xhci_count; i++) {
        xhci_on_interrupt(g_xhci_controllers[i]);
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
                        uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
                        uintptr_t xhci_base = (uintptr_t)(bar0 & 0xFFFFFFF0);
                        if ((bar0 & 0x06) == 0x04) { /* 64-bit BAR */
                            xhci_base |= ((uintptr_t)bar1 << 32);
                        }

                        xhci_controller_t* hcd = (xhci_controller_t*)malloc(sizeof(xhci_controller_t));
                        memset(hcd, 0, sizeof(xhci_controller_t));

                        xhci_controller_init(hcd, xhci_base);

                        if (hcd->controllerStatus == ControllerInitialized) {
                            xhci_initialize_protocols(hcd);
                            xhci_initialize_ports(hcd);

                            if (g_xhci_count < 4) {
                                g_xhci_controllers[g_xhci_count++] = hcd;
                            }

                            /* No-op Test */
                            xhci_noop_command_trb_t testCommand;
                            memset(&testCommand, 0, sizeof(xhci_noop_command_trb_t));
                            testCommand.trbType = TRBTypeNoOpCommand;
                            testCommand.cycleBit = 1;
                            xhci_command_send_raw(&hcd->commandRing, &testCommand);
                            hcd->doorbellRegs[0].doorbell = 0;
                        }
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }

        if (g_xhci_count > 0) {
            /* Register IRQ handler - using vector 43 as a placeholder for USB */
            idt_set_descriptor(43, xhci_irq_handler, 0x8E);
        }
    }
}
