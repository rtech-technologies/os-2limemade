#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vdisk.h"
#include "pci.h"

void serial_write_str(const char* s);

/* AHCI HBA Structures (Physical) */
typedef struct {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  rsv1[4];
} fis_reg_h2d_t;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc:22;
    uint32_t rsv1:9;
    uint32_t i:1;
} hba_prdt_entry_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    hba_prdt_entry_t prdt_entry[1];
} hba_cmd_tbl_t;

typedef struct {
    uint8_t  cfl:5;
    uint8_t  a:1;
    uint8_t  w:1;
    uint8_t  p:1;
    uint8_t  r:1;
    uint8_t  b:1;
    uint8_t  c:1;
    uint8_t  rsv0:1;
    uint8_t  pmp:4;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
} hba_cmd_header_t;

typedef struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t devslp;
    uint32_t rsv1[11]; /* (18 * 4) = 72 bytes. 128 - 72 = 56 bytes. 56 / 4 = 14. */
    uint32_t rsv2[3]; /* More Padding */
} hba_port_t;

typedef struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t bccc;
    uint32_t bccd;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[0x100 - 0x24]; /* Pad to 0x100 where ports start */
    hba_port_t ports[32];
} hba_mem_t;

static hba_mem_t* hba_base = NULL;
static void* port_clb_virt[32];
static void* port_fb_virt[32];

void serial_print_hex(const char* label, uint16_t val);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
void* bump_alloc(size_t size);
uint64_t vmm_get_phys(void* virt);

void ahci_port_start(hba_port_t *port) {
    while (port->cmd & (1 << 15));
    port->cmd |= (1 << 4);
    port->cmd |= (1 << 0);
}

void vga_print(const char* fmt, ...);
void pit_wait_ms(uint32_t ms);

void ahci_force_port_reset(hba_port_t *port, int port_no) {
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;
    port->cmd &= ~0x0001;
    port->cmd &= ~0x0010;

    int engine_timeout = 1000;
    while ((port->cmd & 0x8000 || port->cmd & 0x4000) && engine_timeout--) {
        pit_wait_ms(1);
    }

    port->sctl = (port->sctl & ~0x0F) | 0x301;
    pit_wait_ms(10);
    port->sctl = (port->sctl & ~0x0F) | 0x300;
    pit_wait_ms(50);

    int timeout = 1000;
    while ((port->ssts & 0x0F) != 0x03 && timeout--) {
        pit_wait_ms(1);
    }

    if ((port->ssts & 0x0F) == 0x03) {
        vga_print("[AHCI] PORT %d: LINK ESTABLISHED\n", port_no);
        port->cmd |= 0x0010;
        port->cmd |= 0x0001;
    } else {
        vga_print("[AHCI] PORT %d: MECHANICAL FAILURE\n", port_no);
    }
}

void ahci_hardware_audit(int p) {
    if (!hba_base) return;
    if (!(hba_base->pi & (1 << p))) return;
    hba_port_t* port = &hba_base->ports[p];

    uint32_t ssts = port->ssts;
    if ((ssts & 0x0F) == 0x03) {
        ahci_port_start(port);
    } else {
        ahci_force_port_reset(port, p);
    }
}

uint64_t get_hhdm_offset(void);

int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5;
    cmdhdr->w = 0;
    cmdhdr->prdtl = 1;

    static void* cmdtbl_virt = NULL;
    if (!cmdtbl_virt) cmdtbl_virt = bump_alloc(4096);
    uint64_t cmdtbl_phys = vmm_get_phys(cmdtbl_virt);
    cmdhdr->ctba = (uint32_t)(cmdtbl_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(cmdtbl_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)cmdtbl_virt;
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmdtbl->cfis;
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0x25;
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = (uint8_t)count;
    fis->counth = (uint8_t)(count >> 8);

    port->ci = (1 << 0);
    while (port->ci & (1 << 0)) {
        if (port->tfd & (1 << 0)) return -1;
        __asm__ volatile ("pause");
    }
    return 0;
}

int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5;
    cmdhdr->w = 1;
    cmdhdr->prdtl = 1;

    static void* cmdtbl_virt = NULL;
    if (!cmdtbl_virt) cmdtbl_virt = bump_alloc(4096);
    uint64_t cmdtbl_phys = vmm_get_phys(cmdtbl_virt);
    cmdhdr->ctba = (uint32_t)(cmdtbl_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(cmdtbl_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)cmdtbl_virt;
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmdtbl->cfis;
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0x35;
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = (uint8_t)count;
    fis->counth = (uint8_t)(count >> 8);

    port->ci = (1 << 0);
    while (port->ci & (1 << 0)) {
        if (port->tfd & (1 << 0)) return -1;
        __asm__ volatile ("pause");
    }
    return 0;
}

int atapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5;
    cmdhdr->w = 0;
    cmdhdr->a = 1;
    cmdhdr->prdtl = 1;

    static void* cmdtbl_virt = NULL;
    if (!cmdtbl_virt) cmdtbl_virt = bump_alloc(4096);
    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)cmdtbl_virt;
    uint64_t cmdtbl_phys = vmm_get_phys(cmdtbl_virt);
    cmdhdr->ctba = (uint32_t)(cmdtbl_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(cmdtbl_phys >> 32);

    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = (count * 2048) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    cmdtbl->acmd[0] = 0x28;
    cmdtbl->acmd[1] = 0;
    cmdtbl->acmd[2] = (uint8_t)(lba >> 24);
    cmdtbl->acmd[3] = (uint8_t)(lba >> 16);
    cmdtbl->acmd[4] = (uint8_t)(lba >> 8);
    cmdtbl->acmd[5] = (uint8_t)lba;
    cmdtbl->acmd[6] = 0;
    cmdtbl->acmd[7] = (uint8_t)(count >> 8);
    cmdtbl->acmd[8] = (uint8_t)count;
    cmdtbl->acmd[9] = 0;

    port->ci = (1 << 0);
    while (port->ci & (1 << 0)) {
        if (port->tfd & (1 << 0)) return -1;
        __asm__ volatile ("pause");
    }
    return 0;
}

void ahci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        vga_print("[INIT] Scanning PCI for SATA/AHCI controllers...\n");
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_config_read(bus, slot, func, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;
                uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                uint8_t base_class = (class_info >> 24) & 0xFF;
                uint8_t sub_class = (class_info >> 16) & 0xFF;

                if (base_class == 0x01 && sub_class == 0x06) {
                    pci_enable_master(bus, slot, func);
                    uint32_t bar5 = pci_config_read(bus, slot, func, 0x24);
                    uint64_t hhdm = get_hhdm_offset();
                    hba_base = (hba_mem_t*)(hhdm + (uint64_t)(bar5 & 0xFFFFFFF0));

                    hba_base->ghc |= (1 << 31);
                    hba_base->ghc |= (1 << 0);
                    int ghc_timeout = 1000;
                    while ((hba_base->ghc & (1 << 0)) && ghc_timeout--) pit_wait_ms(1);
                    hba_base->ghc |= (1 << 31);

                    for (int p = 0; p < 32; p++) {
                        if (hba_base->pi & (1 << p)) {
                            port_clb_virt[p] = bump_alloc(1024);
                            uint64_t clb_phys = vmm_get_phys(port_clb_virt[p]);
                            hba_base->ports[p].clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
                            hba_base->ports[p].clbu = (uint32_t)(clb_phys >> 32);
                            port_fb_virt[p] = bump_alloc(256);
                            uint64_t fb_phys = vmm_get_phys(port_fb_virt[p]);
                            hba_base->ports[p].fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
                            hba_base->ports[p].fbu = (uint32_t)(fb_phys >> 32);

                            ahci_force_port_reset(&hba_base->ports[p], p);

                            if ((hba_base->ports[p].ssts & 0x0F) == 0x03) {
                                uint32_t sig = hba_base->ports[p].sig;
                                bool registered_sata = false;

                                if (sig == 0x00000101) { /* SATA */
                                    /* HARDEN: Verification Read to ensure hardware is truly responsive */
                                    uint8_t probe[512];
                                    if (ahci_read_sectors((void*)(uint64_t)p, 0, 1, probe) == 0) {
                                        vdisk_node_t sata_disk = { .sector_size = 512, .total_lba = 1024 * 1024 * 10, .read_lba = ahci_read_sectors, .write_lba = ahci_write_sectors, .private_data = (void*)(uint64_t)p, .is_atapi = false };
                                        register_hardware_disk(sata_disk);
                                        registered_sata = true;
                                        vga_print("[AHCI] Port %d: SATA Verification Success.\n", p);
                                    } else {
                                        vga_print("[AHCI] Port %d: SATA Verification FAILED. Ignoring.\n", p);
                                    }
                                }

                                /* If SATA was not found/functional on this port, only then check for ATAPI */
                                if (!registered_sata && sig == 0xEB140101) { /* ATAPI */
                                    vdisk_node_t cdrom = { .sector_size = 2048, .total_lba = 1024 * 1024, .read_lba = atapi_read_sectors, .write_lba = NULL, .private_data = (void*)(uint64_t)p, .is_atapi = true };
                                    register_hardware_disk(cdrom);
                                    vga_print("[AHCI] Port %d: Registered as ATAPI CD-ROM.\n", p);
                                }
                            }
                        }
                    }
                }
                if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
}
