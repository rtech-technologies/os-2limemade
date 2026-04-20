#include <kernel/libs/core/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <kernel/libs/storage/xhci.h>
#include <include/config.h>
#include <include/kernel/libs/core/lock.h>

void* malloc(size_t size);
void free(void* ptr);
void* pmm_alloc(uint64_t count);
uint64_t vmm_get_phys(void* virt);
uint64_t get_hhdm_offset(void);
void pit_wait_ms(uint32_t ms);
void serial_write_str(const char* s);
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void console_push_char(char c);

static xhci_controller_t* g_xhci_controllers[4];
static int g_xhci_count = 0;

static void local_memset(void* ptr, int val, size_t size) { uint8_t* p = (uint8_t*)ptr; while (size--) *p++ = (uint8_t)val; }
static void local_memcpy(void* dest, const void* src, size_t size) { uint8_t* d = (uint8_t*)dest; const uint8_t* s = (const uint8_t*)src; while (size--) *d++ = *s++; }

void* get_xhci_base(void) { if (g_xhci_count > 0) return (void*)g_xhci_controllers[0]->mmio; return NULL; }

void xhci_ring_init(xhci_ring_t* ring, uint32_t sz) {
    void* p = pmm_alloc(sz / 4096 ? sz / 4096 : 1);
    ring->phys = (uintptr_t)p; ring->virt = (xhci_trb_t*)((uint64_t)p + get_hhdm_offset());
    local_memset(ring->virt, 0, sz); ring->enqueue = 0; ring->max = sz / XHCI_TRB_SIZE - 1; ring->cycle = true;
    xhci_link_trb_t* link = (xhci_link_trb_t*)&ring->virt[ring->max];
    link->segmentPtr = ring->phys; link->trbType = 6; link->cycleBit = 1; link->toggleCycle = 1;
}

void xhci_on_interrupt(xhci_controller_t* hcd) {
    if (!(hcd->op->usbStatus & USB_STS_EINT)) return;
    spin_lock(&hcd->lock);
    hcd->run->interrupters[0].interruptPending = 1; hcd->op->usbStatus |= USB_STS_EINT;
    while (1) {
        xhci_event_trb_t* e = &hcd->ev.ring[hcd->ev.dequeue];
        if (!!e->cycleBit != hcd->ev.cycle) break;
        if (e->trbType == 32) {
            xhci_transfer_event_trb_t* t = (xhci_transfer_event_trb_t*)e;
            if (t->pointer) {
                uint8_t* data = (uint8_t*)(t->pointer + get_hhdm_offset());
                if (t->endpointID == 3) {
                    static char m[256] = { [0x04]='a',[0x05]='b',[0x06]='c',[0x07]='d',[0x08]='e',[0x09]='f',[0x0A]='g',[0x0B]='h',[0x0C]='i',[0x0D]='j',[0x0E]='k',[0x0F]='l',[0x10]='m',[0x11]='n',[0x12]='o',[0x13]='p',[0x14]='q',[0x15]='r',[0x16]='s',[0x17]='t',[0x18]='u',[0x19]='v',[0x1A]='w',[0x1B]='x',[0x1C]='y',[0x1D]='z',[0x1E]='1',[0x1F]='2',[0x20]='3',[0x21]='4',[0x22]='5',[0x23]='6',[0x24]='7',[0x25]='8',[0x26]='9',[0x27]='0',[0x28]='\n',[0x2C]=' ',[0x2A]='\b' };
                    for (int i=2; i<8; i++) if (data[i]) console_push_char(m[data[i]]);
                }
            }
        }
        hcd->ev.dequeue++;
        if (hcd->ev.dequeue >= 256) { hcd->ev.dequeue = 0; hcd->ev.cycle = !hcd->ev.cycle; }
    }
    hcd->run->interrupters[0].eventRingDequeuePointer = (hcd->ev.ring_phys + hcd->ev.dequeue * 16) | XHCI_INT_ERDP_BUSY;
    hcd->run->interrupters[0].interruptPending = 0;
    spin_unlock(&hcd->lock);
}

void xhci_cmd_send(xhci_controller_t* hcd, void* trb) {
    xhci_trb_t* t = (xhci_trb_t*)trb; t->cycleBit = hcd->cmd.cycle;
    hcd->cmd.virt[hcd->cmd.enqueue++] = *t;
    if (hcd->cmd.enqueue >= hcd->cmd.max) { hcd->cmd.enqueue = 0; hcd->cmd.cycle = !hcd->cmd.cycle; }
    hcd->db[0].doorbell = 0;
    int timeout = 100000; while (timeout--) { xhci_on_interrupt(hcd); __asm__ volatile("pause"); }
}

typedef struct { uint32_t sig; uint32_t tag; uint32_t len; uint8_t flg; uint8_t lun; uint8_t cmdlen; uint8_t cmd[16]; } __attribute__((packed)) msc_cbw_t;
typedef struct { uint32_t sig; uint32_t tag; uint32_t res; uint8_t stat; } __attribute__((packed)) msc_csw_t;

int xhci_msc_read(void* priv, uint64_t lba, uint32_t count, void* buf) {
    xhci_device_t* dev = (xhci_device_t*)priv;
    xhci_controller_t* hcd = g_xhci_controllers[0];
    if (!hcd || !dev) return -1;

    msc_cbw_t cbw = { .sig = 0x43425355, .tag = 1, .len = count * 512, .flg = 0x80, .cmdlen = 10 };
    cbw.cmd[0] = 0x28; // READ(10)
    cbw.cmd[2] = (lba>>24)&0xFF; cbw.cmd[3] = (lba>>16)&0xFF; cbw.cmd[4] = (lba>>8)&0xFF; cbw.cmd[5] = lba&0xFF;
    cbw.cmd[7] = (count>>8)&0xFF; cbw.cmd[8] = count&0xFF;

    /* BOT Setup Stage (CBW) - Endpoint 2 Out */
    xhci_trb_t setup = { .parameter = {(uint32_t)vmm_get_phys(&cbw), (uint32_t)(vmm_get_phys(&cbw) >> 32)}, .status = 31, .trbType = 1, .control = (1 << 6) };
    xhci_cmd_send(hcd, &setup);

    /* BOT Data Stage - Endpoint 3 In */
    xhci_trb_t data = { .parameter = {(uint32_t)vmm_get_phys(buf), (uint32_t)(vmm_get_phys(buf) >> 32)}, .status = count * 512, .trbType = 1, .control = (1 << 6) };
    xhci_cmd_send(hcd, &data);

    /* BOT Status Stage (CSW) - Endpoint 3 In */
    msc_csw_t csw;
    xhci_trb_t status = { .parameter = {(uint32_t)vmm_get_phys(&csw), (uint32_t)(vmm_get_phys(&csw) >> 32)}, .status = 13, .trbType = 1, .control = (1 << 6) };
    xhci_cmd_send(hcd, &status);

    if (csw.sig != 0x53425355 || csw.stat != 0) {
        serial_write_str("[USB] MSC BOT Error or Signature Mismatch.\n");
        return -1;
    }

    serial_write_str("[USB] MSC BOT READ(10) Completed.\n");
    return 0;
}

void xhci_setup_device(xhci_controller_t* hcd, uint8_t port) {
    (void)port;
    xhci_trb_t en_slot = { .trbType = 9 }; xhci_cmd_send(hcd, &en_slot);
    xhci_device_t* dev = malloc(sizeof(xhci_device_t)); local_memset(dev, 0, sizeof(xhci_device_t));
    vdisk_node_t stick = { .name = "USB_STICK", .sector_size = 512, .read_lba = xhci_msc_read, .private_data = dev };
    register_hardware_disk(stick);
}

bool xhci_bios_takeover(xhci_controller_t* hcd) {
    uint32_t xecp = (hcd->cap->hccParams1 >> 16) & 0xFFFF; if (!xecp) return true;
    uintptr_t cap_ptr = (uintptr_t)hcd->cap + (xecp << 2);
    while (cap_ptr) {
        uint32_t cap = *(volatile uint32_t*)cap_ptr;
        if ((cap & 0xFF) == 1) {
            *(volatile uint32_t*)cap_ptr |= (1 << 24);
            int timeout = 100; while ((*(volatile uint32_t*)cap_ptr & (1 << 16)) && timeout--) pit_wait_ms(1);
            return !(*(volatile uint32_t*)cap_ptr & (1 << 16));
        }
        uint32_t next = (cap >> 8) & 0xFF; if (!next) break; cap_ptr += (next << 2);
    }
    return true;
}

void xhci_init_controller(xhci_controller_t* hcd, uintptr_t base) {
    hcd->base = base; hcd->mmio = base + get_hhdm_offset();
    hcd->cap = (xhci_cap_regs_t*)hcd->mmio; hcd->op = (xhci_op_regs_t*)(hcd->mmio + hcd->cap->capLength);
    hcd->ports = (xhci_port_regs_t*)(hcd->mmio + hcd->cap->capLength + 0x400);
    hcd->run = (xhci_runtime_regs_t*)(hcd->mmio + (hcd->cap->rtsOff & ~0x1F));
    hcd->db = (xhci_doorbell_register_t*)(hcd->mmio + (hcd->cap->dbOff & ~0x3));
    if (!xhci_bios_takeover(hcd)) return;
    hcd->op->usbCommand &= ~USB_CMD_RS; while (!(hcd->op->usbStatus & USB_STS_HCH)) pit_wait_ms(1);
    hcd->op->usbCommand |= USB_CMD_HCRST; while (hcd->op->usbStatus & USB_STS_CNR) pit_wait_ms(1);
    void* dcbaa_p = pmm_alloc(1); hcd->dcbaa_phys = (uintptr_t)dcbaa_p;
    hcd->dcbaa = (uint64_t*)((uint64_t)dcbaa_p + get_hhdm_offset()); local_memset(hcd->dcbaa, 0, 4096);
    hcd->op->devContextBaseAddrArrayPtr = hcd->dcbaa_phys;
    xhci_ring_init(&hcd->cmd, 4096); hcd->op->cmdRingCtl = hcd->cmd.phys | 1; hcd->op->configure = hcd->cap->hcsParams1 & 0xFF;
    void* erst_p = pmm_alloc(1); hcd->ev.seg_table_phys = (uintptr_t)erst_p;
    hcd->ev.seg_table = (xhci_event_ring_segment_table_entry_t*)((uint64_t)erst_p + get_hhdm_offset());
    void* er_p = pmm_alloc(1); hcd->ev.ring_phys = (uintptr_t)er_p;
    hcd->ev.ring = (xhci_event_trb_t*)((uint64_t)er_p + get_hhdm_offset()); local_memset(hcd->ev.ring, 0, 4096);
    hcd->ev.seg_table[0].ringSegmentBaseAddress = hcd->ev.ring_phys; hcd->ev.seg_table[0].ringSegmentSize = 256;
    hcd->ev.dequeue = 0; hcd->ev.cycle = true;
    hcd->run->interrupters[0].eventRingSegmentTableSize = 1; hcd->run->interrupters[0].eventRingSegmentTableBaseAddress = hcd->ev.seg_table_phys;
    hcd->run->interrupters[0].eventRingDequeuePointer = hcd->ev.ring_phys | XHCI_INT_ERDP_BUSY;
    hcd->run->interrupters[0].interruptEnable = 1; hcd->op->usbStatus |= USB_STS_EINT;
    hcd->op->usbCommand |= USB_CMD_INTE | USB_CMD_RS; while (hcd->op->usbStatus & USB_STS_HCH) pit_wait_ms(1);
    for (int i=0; i<(int)(hcd->cap->hcsParams1>>24); i++) {
        hcd->ports[i].portSC |= (1<<9); hcd->ports[i].portSC |= (1<<4);
        if (hcd->ports[i].portSC & 1) xhci_setup_device(hcd, i+1);
    }
}

void usb_sovereign_task(void) {
    while (1) { for (int i=0; i<g_xhci_count; i++) xhci_on_interrupt(g_xhci_controllers[i]); __asm__ volatile ("int $0x81"); }
}

void xhci_irq_handler(void) { for (int i=0; i<g_xhci_count; i++) xhci_on_interrupt(g_xhci_controllers[i]); }

void usb_xhci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        for (int bus=0; bus<256; bus++) {
            for (int slot=0; slot<32; slot++) {
                for (int func=0; func<8; func++) {
                    uint32_t id = pci_config_read(bus, slot, func, 0x08);
                    if (((id >> 24) & 0xFF) == 0x0C && ((id >> 16) & 0xFF) == 0x03 && ((id >> 8) & 0xFF) == 0x30) {
                        pci_enable_master(bus, slot, func);
                        uint32_t b0 = pci_config_read(bus, slot, func, 0x10);
                        uintptr_t base = b0 & 0xFFFFFFF0;
                        if ((b0 & 0x06) == 0x04) base |= ((uint64_t)pci_config_read(bus, slot, func, 0x14) << 32);
                        xhci_controller_t* hcd = malloc(sizeof(xhci_controller_t)); local_memset(hcd, 0, sizeof(xhci_controller_t));
                        xhci_init_controller(hcd, base); g_xhci_controllers[g_xhci_count++] = hcd;
                    }
                }
            }
        }
        extern void xhci_irq_stub(void);
        if (g_xhci_count) idt_set_descriptor(43, xhci_irq_stub, 0x8E);
    }
}
