#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <include/ahci_hw.h>
#include <include/panic.h>

void vga_print(const char* fmt, ...);
uint64_t vmm_get_phys(void* virt);
uint64_t get_hhdm_offset(void);
void pit_wait_ms(uint32_t ms);
void* memset(void* s, int c, size_t n);

static hba_mem_t* hba_base = NULL;
static void* port_clb_virt[32];
static void* port_fb_virt[32];
static void* port_ctba_virt[32];
static bool ahci_online = false;

hba_mem_t* get_hba_base(void) { return hba_base; }
void* get_port_clb(int p) { return port_clb_virt[p]; }
void* get_port_ctba(int p) { return port_ctba_virt[p]; }
bool ahci_is_ready(void) { return ahci_online; }

#define panic(msg) forensic_panic(msg, NULL)

static void ahci_port_stop(hba_port_t* port) {
    port->cmd &= ~0x0001; /* ST = 0 */
    port->cmd &= ~0x0010; /* FRE = 0 */
    for (volatile int i = 0; i < 1000000; i++) {
        if (!(port->cmd & 0x8000) && !(port->cmd & 0x4000)) break;
        __asm__ volatile ("pause");
    }
}

static void ahci_port_init_addresses(hba_port_t* port, int p) {
    uint64_t clb_p = vmm_get_phys(port_clb_virt[p]);
    port->clb = (uint32_t)clb_p;
    port->clbu = (uint32_t)(clb_p >> 32);

    uint64_t fb_p = vmm_get_phys(port_fb_virt[p]);
    port->fb = (uint32_t)fb_p;
    port->fbu = (uint32_t)(fb_p >> 32);
}

static void ahci_port_start(hba_port_t* port) {
    port->cmd |= 0x0010; /* FRE = 1 */
    port->cmd |= 0x0001; /* ST = 1 */
}

int ahci_wait_status(hba_port_t* port, uint32_t mask, uint32_t expected, uint32_t timeout_loops) {
    for (uint32_t i = 0; i < timeout_loops; i++) {
        if (port->is & (1 << 30)) return -1;
        uint32_t val;
        if (mask == 0x88 || mask == 0x80 || mask == 0x08) val = port->tfd & mask;
        else if (mask == 0x01) val = port->ci & 0x01;
        else val = port->is & mask;
        if (val == expected) return 0;
        if (i % 1000 == 0) {
            bool tasking_is_scanning(void);
            if (!tasking_is_scanning()) sys_yield();
            else pit_wait_ms(1);
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    hba_cmd_header_t* hdr = (hba_cmd_header_t*)port_clb_virt[p];
    hdr->dw0 = 5 | (1 << 16);
    hdr->prdbc = 0;
    hdr->ctba = (uint32_t)vmm_get_phys(port_ctba_virt[p]);
    hdr->ctbau = (uint32_t)(vmm_get_phys(port_ctba_virt[p]) >> 32);
    hba_cmd_tbl_t* tbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
    memset(tbl, 0, sizeof(hba_cmd_tbl_t));
    tbl->prdt_entry[0].dba = (uint32_t)vmm_get_phys(buffer);
    tbl->prdt_entry[0].dbau = (uint32_t)(vmm_get_phys(buffer) >> 32);
    tbl->prdt_entry[0].dw3 = ((count * 512 - 1) & 0x3FFFFF) | (1U << 31);
    uint32_t* fis = (uint32_t*)tbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0x25 << 16);
    fis[1] = (lba & 0xFFFFFF) | (1 << 30);
    fis[2] = (lba >> 24) & 0xFFFFFF;
    fis[3] = count & 0xFFFF;
    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;
    port->ci = 1;
    return ahci_wait_status(port, 1, 0, 1000000);
}

int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    hba_cmd_header_t* hdr = (hba_cmd_header_t*)port_clb_virt[p];
    hdr->dw0 = 5 | (1 << 6) | (1 << 16);
    hdr->prdbc = 0;
    hdr->ctba = (uint32_t)vmm_get_phys(port_ctba_virt[p]);
    hdr->ctbau = (uint32_t)(vmm_get_phys(port_ctba_virt[p]) >> 32);
    hba_cmd_tbl_t* tbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
    memset(tbl, 0, sizeof(hba_cmd_tbl_t));
    tbl->prdt_entry[0].dba = (uint32_t)vmm_get_phys(buffer);
    tbl->prdt_entry[0].dbau = (uint32_t)(vmm_get_phys(buffer) >> 32);
    tbl->prdt_entry[0].dw3 = ((count * 512 - 1) & 0x3FFFFF) | (1U << 31);
    uint32_t* fis = (uint32_t*)tbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0x35 << 16);
    fis[1] = (lba & 0xFFFFFF) | (1 << 30);
    fis[2] = (lba >> 24) & 0xFFFFFF;
    fis[3] = count & 0xFFFF;
    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;
    port->ci = 1;
    return ahci_wait_status(port, 1, 0, 1000000);
}

int satapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
int satapi_eject(void* priv);
int satapi_identify(void* priv);
int satapi_check_medium(void* priv);
int satapi_read_capacity(void* priv, uint32_t* out_lba, uint32_t* out_ss);
int rtech_iso_init(int drive);

void ahci_service(kernel_event_t event) {
    if (event != EVENT_INIT || hba_base) return;
    vga_print("[AHCI] Sovereign Foundary: Scanning PCI...\n");
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                if (((class_info >> 24) & 0xFF) == 0x01 && ((class_info >> 16) & 0xFF) == 0x06) {
                    pci_enable_master(bus, slot, func);
                    uint32_t bar5 = pci_config_read(bus, slot, func, 0x24);
                    hba_base = (hba_mem_t*)(get_hhdm_offset() + (uint64_t)(bar5 & 0xFFFFFFF0));
                    hba_base->ghc |= (1 << 31); hba_base->ghc |= (1 << 0);
                    for (volatile int i=0; i<100; i++) { if (!(hba_base->ghc & 1)) break; pit_wait_ms(1); }
                    hba_base->ghc |= (1 << 31);
                    vga_print("[AHCI] Controller Ready.\n");

                    void* slab_alloc_aligned(int id, size_t size, size_t align);
                    for (int p = 0; p < 32; p++) {
                        if (hba_base->pi & (1 << p)) {
                            hba_port_t* port = &hba_base->ports[p];
                            ahci_port_stop(port);
                            port_clb_virt[p] = slab_alloc_aligned(0, 1024, 4096);
                            port_fb_virt[p] = slab_alloc_aligned(0, 256, 4096);
                            port_ctba_virt[p] = slab_alloc_aligned(0, 4096, 4096);

                            ahci_port_init_addresses(port, p);

                            port->sctl = (port->sctl & ~0x0F) | 0x301;
                            pit_wait_ms(10);
                            port->sctl = (port->sctl & ~0x0F) | 0x300;
                            pit_wait_ms(50);

                            for (int i=0; i<100; i++) { if ((port->ssts & 0x0F) == 0x03) break; pit_wait_ms(1); }
                            if ((port->ssts & 0x0F) == 0x03) {
                                vga_print("[AHCI] Port %d: Link Active.\n", p);
                                port->serr = 0xFFFFFFFF;
                                ahci_port_start(port);
                                for (int i=0; i<1000; i++) { if (port->sig != 0xFFFFFFFF) break; pit_wait_ms(1); }
                                if (port->sig == 0x00000101) {
                                    vdisk_node_t d = { .name = "SATA_HDD", .sector_size = 512, .total_lba = 1024*1024*10, .partition_offset = 2048, .read_lba = ahci_read_sectors, .write_lba = ahci_write_sectors, .private_data = (void*)(uint64_t)p };
                                    register_hardware_disk(d);
                                    vga_print("[AHCI] SATA HDD Online.\n");
                                    ahci_online = true;
                                } else if (port->sig == 0xEB140101) {
                                    vdisk_node_t d = { .name = "SATA_CD", .sector_size = 2048, .read_lba = satapi_read_sectors, .eject = satapi_eject, .private_data = (void*)(uint64_t)p, .is_atapi = true };
                                    register_hardware_disk(d);
                                    vga_print("[AHCI] ATAPI CDROM Online.\n");
                                    rtech_iso_init(get_hw_disk_count() - 1);
                                    ahci_online = true;
                                }
                            }
                        }
                    }
                    return;
                }
                if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
            }
        }
    }
}

void ahci_hardware_audit(int p) {
    if (!hba_base || !(hba_base->pi & (1 << p))) return;
    hba_port_t* port = &hba_base->ports[p];
    if (port->tfd & 0x88) return;
    if ((port->ssts & 0x0F) == 0x03 && !(port->cmd & 0x0001)) ahci_port_start(port);
}

int ahci_mechanical_sync(int p) { (void)p; return 0; }
void ahci_scan_remaining(void) { }
