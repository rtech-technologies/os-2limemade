#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vdisk.h"

void serial_write_str(const char* s);
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

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
    uint8_t  rsv[0xA0-0x28];
    uint8_t  vendor[0x100-0xA0];
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
    /* 1. Wait for bit 15 (Command List Running) to clear */
    while (port->cmd & (1 << 15));

    /* 2. Set bit 4 (FIS Receive Enable) and bit 0 (Start) */
    port->cmd |= (1 << 4);
    port->cmd |= (1 << 0);
}

void vga_print(const char* fmt, ...);

void pit_wait_ms(uint32_t ms);

void ahci_force_port_reset(hba_port_t *port, int port_no) {
    /* 1. CLEAR: Purge the Error register at the very start to acknowledge noise */
    /* In AHCI, writing 1 to these bits CLEARS them. */
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    /* 2. STOP: Kill the DMA engines (ST and FRE) */
    port->cmd &= ~0x0001; /* Bit 0: ST (Start) */
    port->cmd &= ~0x0010; /* Bit 4: FRE (FIS Receive Enable) */

    /* Wait for the engines to actually stop (CR and FR bits) */
    int engine_timeout = 1000;
    while ((port->cmd & 0x8000 || port->cmd & 0x4000) && engine_timeout--) {
        pit_wait_ms(1);
    }

    /* 3. KICK: The COMRESET (SCTL) */
    /* Bit 0-3 = 1 (Perform Reset), Bit 4-7 = 3 (No Power Management) */
    /* 0x301 Forces 1.5/3.0 Gbps handshake + Reset */
    port->sctl = (port->sctl & ~0x0F) | 0x301;

    /* Hardware Delay: Give the hardware 1ms to physically reset */
    pit_wait_ms(1);

    port->sctl = (port->sctl & ~0x0F) | 0x300; /* End Reset (Back to normal Operation) */

    /* 4. WAIT: The 1-Second Negotiation Loop */
    int timeout = 1000;
    while ((port->ssts & 0x0F) != 0x03 && timeout--) {
        pit_wait_ms(1);
    }

    if ((port->ssts & 0x0F) == 0x03) {
        vga_print("[AHCI] PORT %d: LINK ESTABLISHED (SSTS: 0x%x, SERR: 0x%x)\n", port_no, port->ssts, port->serr);
        /* Now it's safe to set the Command List and FIS addresses */
        port->cmd |= 0x0010; /* FRE */
        port->cmd |= 0x0001; /* ST */
    } else {
        vga_print("[AHCI] PORT %d: MECHANICAL FAILURE (SSTS: 0x%x, SERR: 0x%x)\n", port_no, port->ssts, port->serr);
    }
}

void ahci_hardware_audit(int p) {
    if (!hba_base) return;
    if (!(hba_base->pi & (1 << p))) return;
    hba_port_t* port = &hba_base->ports[p];

    /* Audit GHC (Global Host Control) */
    uint32_t ghc = hba_base->ghc;
    serial_print_hex("[AHCI] GHC Status: ", (uint16_t)(ghc >> 16));
    serial_print_hex("", (uint16_t)ghc);

    /* Audit Port SSTS (SATA Status) */
    uint32_t ssts = port->ssts;
    serial_print_hex("[AHCI] Port SSTS: ", (uint16_t)ssts);

    if ((ssts & 0x0F) == 0x03) {
        vga_print("[AHCI] SATA Hardware Online. Link Established.\n");
        ahci_port_start(port);
    } else if ((ssts & 0x0F) == 0x01 || (ssts & 0x0F) == 0x00) {
        /* If SSTS 0x01 (detected but no link) or even 0x00 (in case it's just stuck), try handshake */
        vga_print("[AHCI] Port %d: Attempting Hardware Handshake...\n", p);
        ahci_force_port_reset(port, p);
    } else {
        vga_print("[AHCI] MECHANICAL ERROR: Port %d SSTS: 0x%x\n", p, ssts);
    }
}

uint64_t get_hhdm_offset(void);

int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    if (!(hba_base->pi & (1 << p))) return -1;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t vmm_get_phys(void* virt);

    /* THE FIX: Convert 'buffer' (Virtual) to 'phys_buffer' (Physical) */
    uint64_t phys_buffer = vmm_get_phys(buffer);

    /* 1. Command Header Setup */
    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5; /* 5 DWORDs */
    cmdhdr->w = 0;   /* Read */
    cmdhdr->prdtl = 1;

    /* 2. Command Table / PRDT Setup */
    /* For simplicity, we'll reuse a fixed area or allocate one */
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

    /* 3. Setup Command FIS (H2D) */
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmdtbl->cfis;
    fis->fis_type = 0x27; /* H2D */
    fis->c = 1;
    fis->command = 0x25; /* READ DMA EXT */
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6; /* LBA mode */
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = (uint8_t)count;
    fis->counth = (uint8_t)(count >> 8);

    /* 4. Issue Command */
    port->ci = (1 << 0);
    while (port->ci & (1 << 0)) {
        if (port->tfd & (1 << 0)) {
            vga_print("[AHCI] PORT %d READ ERROR: TFD 0x%x (LBA %d)\n", p, port->tfd, (uint32_t)lba);
            return -1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    if (!(hba_base->pi & (1 << p))) return -1;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t vmm_get_phys(void* virt);

    /* THE FIX: Convert 'buffer' (Virtual) to 'phys_buffer' (Physical) */
    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5;
    cmdhdr->w = 1; /* Write */
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
    fis->command = 0x35; /* WRITE DMA EXT */
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
        if (port->tfd & (1 << 0)) {
            vga_print("[AHCI] PORT %d WRITE ERROR: TFD 0x%x (LBA %d)\n", p, port->tfd, (uint32_t)lba);
            return -1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

int atapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    if (!(hba_base->pi & (1 << p))) return -1;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t vmm_get_phys(void* virt);

    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5;
    cmdhdr->w = 0;
    cmdhdr->a = 1; /* ATAPI */
    cmdhdr->prdtl = 1;

    static void* cmdtbl_virt = NULL;
    if (!cmdtbl_virt) cmdtbl_virt = bump_alloc(4096);
    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)cmdtbl_virt;

    uint64_t cmdtbl_phys = vmm_get_phys(cmdtbl_virt);
    cmdhdr->ctba = (uint32_t)(cmdtbl_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(cmdtbl_phys >> 32);

    /* PRDT Setup: ATAPI uses 2048-byte sectors usually */
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = (count * 2048) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    /* ATAPI Packet: SCSI READ(10) */
    cmdtbl->acmd[0] = 0x28; /* GPCMD_READ_10 */
    cmdtbl->acmd[1] = 0;
    cmdtbl->acmd[2] = (uint8_t)(lba >> 24);
    cmdtbl->acmd[3] = (uint8_t)(lba >> 16);
    cmdtbl->acmd[4] = (uint8_t)(lba >> 8);
    cmdtbl->acmd[5] = (uint8_t)lba;
    cmdtbl->acmd[6] = 0;
    cmdtbl->acmd[7] = (uint8_t)(count >> 8);
    cmdtbl->acmd[8] = (uint8_t)count;
    cmdtbl->acmd[9] = 0;

    /* Issue Command */
    port->ci = (1 << 0);
    while (port->ci & (1 << 0)) {
        if (port->tfd & (1 << 0)) {
            vga_print("[AHCI] ATAPI PORT %d READ ERROR: TFD 0x%x\n", p, port->tfd);
            return -1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

void ahci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        vga_print("[INIT] Scanning PCI for SATA/AHCI controllers...\n");
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                uint32_t vendor_device = pci_config_read(bus, slot, 0, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                uint32_t class_info = pci_config_read(bus, slot, 0, 0x08);
                uint8_t base_class = (class_info >> 24) & 0xFF;
                uint8_t sub_class = (class_info >> 16) & 0xFF;

                if (base_class == 0x01 && sub_class == 0x06) { /* Mass Storage, SATA */
                    vga_print("[INIT] Found AHCI Controller.\n");
                    pci_enable_master(bus, slot, 0);

                    uint32_t bar5 = pci_config_read(bus, slot, 0, 0x24);
                    uint64_t hhdm = get_hhdm_offset();
                    hba_base = (hba_mem_t*)(hhdm + (uint64_t)(bar5 & 0xFFFFFFF0));

                    /* Scan HBA Ports */
                    for (int p = 0; p < 32; p++) {
                        if (hba_base->pi & (1 << p)) {
                            /* Initial setup of addresses must be physical */
                            port_clb_virt[p] = bump_alloc(1024);
                            uint64_t clb_phys = vmm_get_phys(port_clb_virt[p]);
                            hba_base->ports[p].clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
                            hba_base->ports[p].clbu = (uint32_t)(clb_phys >> 32);

                            port_fb_virt[p] = bump_alloc(256);
                            uint64_t fb_phys = vmm_get_phys(port_fb_virt[p]);
                            hba_base->ports[p].fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
                            hba_base->ports[p].fbu = (uint32_t)(fb_phys >> 32);

                            uint32_t sig = hba_base->ports[p].sig;
                            if (sig == 0x00000101) { /* SATA */
                                vga_print("[INIT] Port %d detected: SATA Hard Disk.\n", p);

                                /* Aggressive Reset to ensure Link 0x3 */
                                ahci_force_port_reset(&hba_base->ports[p], p);

                                if ((hba_base->ports[p].ssts & 0x0F) == 0x03) {
                                    vdisk_node_t sata_disk = {
                                        .sector_size = 512,
                                        .total_lba = 1024 * 1024 * 10,
                                        .read_lba = ahci_read_sectors,
                                        .write_lba = ahci_write_sectors,
                                    .private_data = (void*)(uint64_t)p,
                                    .is_atapi = false
                                    };
                                    register_hardware_disk(sata_disk);
                                }
                            } else if (sig == 0xEB140101) { /* ATAPI */
                                vga_print("[INIT] Port %d detected: ATAPI CD-ROM.\n", p);
                                vdisk_node_t cdrom = {
                                    .sector_size = 2048,
                                    .total_lba = 1024 * 1024,
                                    .read_lba = atapi_read_sectors,
                                    .write_lba = NULL,
                                    .private_data = (void*)(uint64_t)p,
                                    .is_atapi = true
                                };
                                register_hardware_disk(cdrom);
                            }
                        }
                    }
                }
            }
        }
    }
}
