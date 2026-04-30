#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <include/ahci_hw.h>
#include <include/panic.h>
#include <include/string.h>
#include <include/stdlib.h>
#include <include/timer.h>

void vga_print(const char* fmt, ...);
uint64_t get_hhdm_offset(void);

hba_mem_t* hba_base = NULL;
void* port_clb_virt[32];
void* port_fb_virt[32];
void* port_ctba_virt[32];
static uint32_t g_registered_ports_mask = 0;
static bool ahci_ready = false;

void* get_port_clb(int p) { return port_clb_virt[p]; }
void* get_port_ctba(int p) { return port_ctba_virt[p]; }
bool ahci_is_ready(void) { return ahci_ready; }
hba_mem_t* get_hba_base(void) { return hba_base; }

uint64_t vmm_get_phys(void* virt);
void ahci_force_port_reset(int port_no);
#define virtual_to_physical(virt) vmm_get_phys(virt)

void register_hardware_disk_from_port(int p);

int ahci_wait_status(hba_port_t* port, uint32_t mask, uint32_t expected, uint32_t timeout_loops) {
    for (uint32_t i = 0; i < timeout_loops; i++) {
        if (port->is & (1 << 30)) return -1;
        if (mask == (1 << 0)) { if ((port->ci & mask) == expected) return 0; }
        else if (port->is & mask) { port->is = 0xFFFFFFFF; return 0; }
        else if ((port->tfd & 0x88) == 0) return 0;

        bool tasking_is_scanning(void);
        if (!tasking_is_scanning()) sys_yield();
        else if (i % 10000 == 0) vga_print(".");
    }
    return -1;
}

void ahci_port_stop(int p) {
    hba_port_t* port = &hba_base->ports[p];
    port->cmd &= ~0x0001;
    int timeout = 500000;
    while ((port->cmd & 0x8000) && timeout--) __asm__ volatile ("pause");
    port->cmd &= ~0x0010;
    timeout = 500000;
    while ((port->cmd & 0x4000) && timeout--) __asm__ volatile ("pause");
}

void ahci_port_start(int p) {
    hba_port_t* port = &hba_base->ports[p];
    ahci_port_stop(p);
    uint64_t clb_phys = virtual_to_physical(port_clb_virt[p]);
    port->clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)(clb_phys >> 32);
    uint64_t fb_phys = virtual_to_physical(port_fb_virt[p]);
    port->fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)(fb_phys >> 32);
    port->cmd = (port->cmd & ~0xF0000000) | 0x10000000 | (1 << 1);
    port->serr = 0xFFFFFFFF; port->is = 0xFFFFFFFF;
    port->cmd |= (1 << 4) | (1 << 0);
    vga_print("[AHCI] Port %d started.\n", p);
}

void ahci_force_port_reset(int port_no) {
    hba_port_t* port = &hba_base->ports[port_no];
    ahci_port_stop(port_no);
    port->sctl = (port->sctl & ~0x0F) | 0x01;
    for(int i=0; i<1000000; i++) __asm__ volatile("pause");
    port->sctl = (port->sctl & ~0x0F) | 0x00;
    int timeout = 1000000;
    while ((port->ssts & 0x0F) != 0x03 && timeout--) __asm__ volatile ("pause");
    ahci_port_start(port_no);
}

int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t phys_buffer = virtual_to_physical(buffer);
    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->dw0 = 5 | (1 << 16); cmdhdr->prdbc = 0;
    uint64_t ctba_phys = virtual_to_physical(port_ctba_virt[p]);
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);
    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dw3 = ((count * 512 - 1) & 0x3FFFFF) | (1U << 31);
    uint32_t* fis = (uint32_t*)cmdtbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0x25 << 16);
    fis[1] = (lba & 0xFFFFFF) | (0x40 << 24);
    fis[2] = (lba >> 24) & 0xFFFFFF; fis[3] = count & 0xFFFF;
    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;
    port->ci = (1 << 0);
    return ahci_wait_status(port, (1 << 0), 0, 1000000);
}

int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t phys_buffer = virtual_to_physical(buffer);
    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->dw0 = 5 | (1 << 6) | (1 << 16); cmdhdr->prdbc = 0;
    uint64_t ctba_phys = virtual_to_physical(port_ctba_virt[p]);
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);
    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dw3 = ((count * 512 - 1) & 0x3FFFFF) | (1U << 31);
    uint32_t* fis = (uint32_t*)cmdtbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0x35 << 16);
    fis[1] = (lba & 0xFFFFFF) | (0x40 << 24);
    fis[2] = (lba >> 24) & 0xFFFFFF; fis[3] = count & 0xFFFF;
    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;
    port->ci = (1 << 0);
    return ahci_wait_status(port, (1 << 0), 0, 1000000);
}

int satapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
int satapi_eject(void* priv);
int satapi_identify(void* priv);
int satapi_check_medium(void* priv);
int satapi_read_capacity(void* priv, uint32_t* out_lba, uint32_t* out_ss);
int rtech_iso_init(int drive);

void register_hardware_disk_from_port(int p) {
    if (g_registered_ports_mask & (1 << p)) return;
    uint32_t sig = hba_base->ports[p].sig;
    if (sig == 0x00000101) {
        uint8_t* sector = malloc(512);
        if (ahci_read_sectors((void*)(uint64_t)p, 1, 1, sector) == 0 && *(uint64_t*)sector == 0x5452415020494645ULL) {
            if (ahci_read_sectors((void*)(uint64_t)p, 2, 1, sector) == 0) {
                for (int i = 0; i < 4; i++) {
                    uint8_t* entry = &sector[i * 128];
                    uint64_t start_lba = *(uint64_t*)&entry[32];
                    uint64_t end_lba = *(uint64_t*)&entry[40];
                    if (start_lba == 0) continue;
                    vdisk_node_t part = { .sector_size = 512, .total_lba = end_lba - start_lba + 1, .partition_offset = start_lba, .read_lba = ahci_read_sectors, .write_lba = ahci_write_sectors, .private_data = (void*)(uint64_t)p, .is_atapi = false };
                    part.name[0]='S';part.name[1]='A';part.name[2]='T';part.name[3]='A';part.name[4]='0'+p;part.name[5]='_';part.name[6]='P';part.name[7]='0'+i;part.name[8]='\0';
                    register_hardware_disk(part);
                }
            }
        } else {
            vdisk_node_t sata_disk = { .name = "SATA_HDD", .sector_size = 512, .total_lba = 1024*1024*10, .partition_offset = 2048, .read_lba = ahci_read_sectors, .write_lba = ahci_write_sectors, .private_data = (void*)(uint64_t)p, .is_atapi = false };
            register_hardware_disk(sata_disk);
        }
        free(sector); g_registered_ports_mask |= (1 << p);
    } else if (sig == 0xEB140101) {
        vdisk_node_t cdrom = { .name = "SATA_CD", .sector_size = 2048, .total_lba = 0, .partition_offset = 0, .read_lba = satapi_read_sectors, .write_lba = NULL, .eject = satapi_eject, .private_data = (void*)(uint64_t)p, .is_atapi = true };
        if (satapi_identify(cdrom.private_data) == 0 && satapi_check_medium(cdrom.private_data) == 0) {
            uint32_t max_lba, block_size;
            if (satapi_read_capacity(cdrom.private_data, &max_lba, &block_size) == 0) { cdrom.total_lba = (uint64_t)max_lba + 1; cdrom.sector_size = block_size; }
        }
        register_hardware_disk(cdrom); g_registered_ports_mask |= (1 << p);
        rtech_iso_init(get_hw_disk_count() - 1);
    }
}

static void ahci_init_port_hw(int p) {
    void* slab_alloc_aligned(int id, size_t size, size_t align);
    port_clb_virt[p] = slab_alloc_aligned(0, 1024, 1024);
    port_fb_virt[p] = slab_alloc_aligned(0, 4096, 256);
    port_ctba_virt[p] = slab_alloc_aligned(0, 4096, 4096);
    if (!port_clb_virt[p] || !port_fb_virt[p] || !port_ctba_virt[p]) return;
    ahci_force_port_reset(p);
}

void ahci_hardware_audit(int p) {
    if (!hba_base) return;
    if (!(hba_base->pi & (1 << p))) return;
    hba_port_t* port = &hba_base->ports[p];
    if (g_registered_ports_mask & (1 << p)) return;
    static uint64_t last_audit_ticks[32] = {0};
    uint64_t now = get_system_ticks();
    if (now - last_audit_ticks[p] < 5000) return;
    last_audit_ticks[p] = now;
    if (port->tfd & 0x88) return;
    if ((port->ssts & 0x0F) == 0x03) {
        ahci_port_start(p);
        if (port->sig == 0x00000101 || port->sig == 0xEB140101) register_hardware_disk_from_port(p);
    } else {
        ahci_force_port_reset(p);
    }
}

void ahci_scan_remaining(void) {
    if (!hba_base) return;
    for (int p = 5; p < 32; p++) {
        if (hba_base->pi & (1 << p)) {
            ahci_init_port_hw(p);
            if ((hba_base->ports[p].ssts & 0x0F) == 0x03) register_hardware_disk_from_port(p);
            sys_yield();
        }
    }
}

void ahci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        if (hba_base != NULL) return;
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                    uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                    if (((class_info >> 24) & 0xFF) == 0x01 && ((class_info >> 16) & 0xFF) == 0x06) {
                        pci_enable_master(bus, slot, func);
                        uint32_t bar5 = pci_config_read(bus, slot, func, 0x24);
                        hba_base = (hba_mem_t*)(get_hhdm_offset() + (uint64_t)(bar5 & 0xFFFFFFF0));
                        vga_print("[AHCI] Controller BAR5: 0x%x\n", (uint32_t)bar5);
                        if (hba_base->cap2 & (1 << 0)) {
                            vga_print("[AHCI] Requesting BIOS Handoff...\n");
                            hba_base->bohc |= (1 << 1);
                            int bohc_ms = 0;
                            while ((hba_base->bohc & (1 << 0)) && bohc_ms < 1000) { pit_wait_ms(1); bohc_ms++; }
                        }
                        hba_base->ghc |= (1 << 31); hba_base->ghc |= (1 << 0);
                        int ghc_ms = 0;
                        while ((hba_base->ghc & (1 << 0)) && ghc_ms < 1000) { pit_wait_ms(1); ghc_ms++; }
                        hba_base->ghc |= (1 << 31);
                        vga_print("[AHCI] HBA Reset Complete.\n");
                        for (int p = 0; p < 5; p++) {
                            if (hba_base->pi & (1 << p)) {
                                ahci_init_port_hw(p);
                                if ((hba_base->ports[p].ssts & 0x0F) == 0x03) register_hardware_disk_from_port(p);
                            }
                        }
                        ahci_ready = true; return;
                    }
                    if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
}
