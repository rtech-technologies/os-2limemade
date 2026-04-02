#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>

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
static void* port_ctba_virt[32];

void* get_port_clb(int p) { return port_clb_virt[p]; }
void* get_port_ctba(int p) { return port_ctba_virt[p]; }
hba_mem_t* get_hba_base(void) { return hba_base; }

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

    /* SATA Test for ATAPI: Reject generic SATA reads on ATAPI signatures */
    if (port->sig == 0xEB140101) {
        vga_print("[AHCI] Port %d: Rejected SATA Read on ATAPI device.\n", p);
        return -1;
    }

    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5;
    cmdhdr->w = 0;
    cmdhdr->a = 0; /* Pure SATA */
    cmdhdr->prdtl = 1;

    /* Re-link Command Table every time to ensure atomic correctness */
    uint64_t ctba_phys = vmm_get_phys(port_ctba_virt[p]);
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
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

    /* Idle Wait: Wait for drive to be ready to receive command */
    int timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && timeout--) {
        __asm__ volatile ("pause");
    }

    port->ci = (1 << 0);
    while ((port->ci & (1 << 0)) && timeout--) {
        if (port->tfd & (1 << 0)) { /* ERR bit */
            vga_print("[AHCI] Port %d READ ERROR: TFD 0x%x\n", p, port->tfd);
            return -1;
        }
        __asm__ volatile ("pause");
    }
    if (timeout <= 0) {
        vga_print("[AHCI] Port %d READ TIMEOUT\n", p);
        return -1;
    }
    return 0;
}

int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];

    /* SATA Test for ATAPI: Reject generic SATA writes on ATAPI signatures */
    if (port->sig == 0xEB140101) {
        vga_print("[AHCI] Port %d: Rejected SATA Write on ATAPI device.\n", p);
        return -1;
    }

    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->cfl = 5;
    cmdhdr->w = 1;
    cmdhdr->a = 0; /* Pure SATA */
    cmdhdr->prdtl = 1;

    /* Re-link Command Table every time to ensure atomic correctness */
    uint64_t ctba_phys = vmm_get_phys(port_ctba_virt[p]);
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
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

    /* Idle Wait: Wait for drive to be ready to receive command */
    int timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && timeout--) {
        __asm__ volatile ("pause");
    }

    port->ci = (1 << 0);
    while ((port->ci & (1 << 0)) && timeout--) {
        if (port->tfd & (1 << 0)) {
            vga_print("[AHCI] Port %d WRITE ERROR: TFD 0x%x\n", p, port->tfd);
            return -1;
        }
        __asm__ volatile ("pause");
    }
    if (timeout <= 0) {
        vga_print("[AHCI] Port %d WRITE TIMEOUT\n", p);
        return -1;
    }
    return 0;
}

int satapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
int satapi_eject(void* priv);
int satapi_identify(void* priv);
int satapi_check_medium(void* priv);
int satapi_read_capacity(void* priv, uint32_t* out_lba, uint32_t* out_ss);

void ahci_scan_remaining(void) {
    if (!hba_base) return;
    vga_print("[AHCI] Performing extended scan (Ports 9-31)...\n");
    for (int p = 9; p < 32; p++) {
        if (hba_base->pi & (1 << p)) {
            /* CLB Alignment: AHCI Command Lists must be 1KB aligned */
            void* raw_clb = bump_alloc(1024 + 1024);
            if (raw_clb) {
                uint64_t addr = (uint64_t)raw_clb;
                if (addr % 1024 != 0) addr = (addr + 1023) & ~1023;
                port_clb_virt[p] = (void*)addr;
            } else port_clb_virt[p] = NULL;

            port_fb_virt[p] = bump_alloc(256);
            port_ctba_virt[p] = bump_alloc(4096);

            if (!port_clb_virt[p] || !port_fb_virt[p] || !port_ctba_virt[p]) {
                vga_print("[AHCI] FATAL: Port %d Heap Allocation Failure.\n", p);
                continue;
            }

            uint64_t clb_phys = vmm_get_phys(port_clb_virt[p]);
            hba_base->ports[p].clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
            hba_base->ports[p].clbu = (uint32_t)(clb_phys >> 32);

            uint64_t fb_phys = vmm_get_phys(port_fb_virt[p]);
            hba_base->ports[p].fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
            hba_base->ports[p].fbu = (uint32_t)(fb_phys >> 32);

            uint64_t ctba_phys = vmm_get_phys(port_ctba_virt[p]);
            hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
            cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
            cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

            ahci_force_port_reset(&hba_base->ports[p], p);

            /* Signature Delay: Wait for hardware to update registers after reset */
            pit_wait_ms(10);

            if ((hba_base->ports[p].ssts & 0x0F) == 0x03) {
                uint32_t sig = hba_base->ports[p].sig;
                if (sig == 0x00000101) { /* SATA */
                    vdisk_node_t sata_disk = {
                        .name = "SATA_HDD",
                        .sector_size = 512,
                        .total_lba = 1024 * 1024 * 10,
                        .partition_offset = 2048, /* GPT Sovereignty Offset */
                        .read_lba = ahci_read_sectors,
                        .write_lba = ahci_write_sectors,
                        .private_data = (void*)(uint64_t)p,
                        .is_atapi = false
                    };
                    register_hardware_disk(sata_disk);
                    vga_print("[AHCI] Port %d: SATA Hard Disk Online.\n", p);
                } else if (sig == 0xEB140101) { /* ATAPI */
                    vdisk_node_t cdrom = {
                        .name = "SATA_CD",
                        .sector_size = 2048,
                        .total_lba = 0,
                        .partition_offset = 0,
                        .read_lba = satapi_read_sectors,
                        .write_lba = NULL,
                        .eject = satapi_eject,
                        .private_data = (void*)(uint64_t)p,
                        .is_atapi = true
                    };

                    /* IDENTIFY PACKET DEVICE logic */
                    if (satapi_identify(cdrom.private_data) == 0 &&
                        satapi_check_medium(cdrom.private_data) == 0) {
                        uint32_t max_lba, block_size;
                        if (satapi_read_capacity(cdrom.private_data, &max_lba, &block_size) == 0) {
                            cdrom.total_lba = (uint64_t)max_lba + 1;
                            cdrom.sector_size = block_size;
                        }
                    } else {
                        /* Label as No Medium */
                        int k = 0; const char* tag = " [NO MEDIUM]";
                        while(cdrom.name[k]) k++;
                        while(*tag) cdrom.name[k++] = *tag++;
                        cdrom.name[k] = '\0';
                    }

                    register_hardware_disk(cdrom);
                    vga_print("[AHCI] Port %d: ATAPI/SCSI Device Online.\n", p);
                }
            }
        }
    }
}

void ahci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        if (hba_base != NULL) return; /* Shield: Already Initialized */

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

                    /* OSx2: Only scan the first 9 ports on boot to save time */
                    for (int p = 0; p < 9; p++) {
                        if (hba_base->pi & (1 << p)) {
                            /* CLB Alignment: AHCI Command Lists must be 1KB aligned */
                            void* raw_clb = bump_alloc(1024 + 1024);
                            if (raw_clb) {
                                uint64_t addr = (uint64_t)raw_clb;
                                if (addr % 1024 != 0) addr = (addr + 1023) & ~1023;
                                port_clb_virt[p] = (void*)addr;
                            } else port_clb_virt[p] = NULL;

                            port_fb_virt[p] = bump_alloc(256);
                            port_ctba_virt[p] = bump_alloc(4096);

                            if (!port_clb_virt[p] || !port_fb_virt[p] || !port_ctba_virt[p]) {
                                vga_print("[AHCI] FATAL: Port %d Heap Allocation Failure.\n", p);
                                continue;
                            }

                            uint64_t clb_phys = vmm_get_phys(port_clb_virt[p]);
                            hba_base->ports[p].clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
                            hba_base->ports[p].clbu = (uint32_t)(clb_phys >> 32);

                            uint64_t fb_phys = vmm_get_phys(port_fb_virt[p]);
                            hba_base->ports[p].fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
                            hba_base->ports[p].fbu = (uint32_t)(fb_phys >> 32);

                            uint64_t ctba_phys = vmm_get_phys(port_ctba_virt[p]);
                            hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
                            cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
                            cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

                            ahci_force_port_reset(&hba_base->ports[p], p);

                            /* Signature Delay: Wait for hardware to update registers after reset */
                            pit_wait_ms(10);

                            if ((hba_base->ports[p].ssts & 0x0F) == 0x03) {
                                uint32_t sig = hba_base->ports[p].sig;
                                if (sig == 0x00000101) { /* SATA */
                                    vdisk_node_t sata_disk = {
                                        .name = "SATA_HDD",
                                        .sector_size = 512,
                                        .total_lba = 1024 * 1024 * 10,
                                        .partition_offset = 2048, /* GPT Sovereignty Offset */
                                        .read_lba = ahci_read_sectors,
                                        .write_lba = ahci_write_sectors,
                                        .private_data = (void*)(uint64_t)p,
                                        .is_atapi = false
                                    };
                                    register_hardware_disk(sata_disk);
                                    vga_print("[AHCI] Port %d: SATA Hard Disk Online.\n", p);
                                } else if (sig == 0xEB140101) { /* ATAPI */
                                    vdisk_node_t cdrom = {
                                        .name = "SATA_CD",
                                        .sector_size = 2048,
                        .total_lba = 0,
                        .partition_offset = 0,
                        .read_lba = satapi_read_sectors,
                                        .write_lba = NULL,
                        .eject = satapi_eject,
                                        .private_data = (void*)(uint64_t)p,
                                        .is_atapi = true
                                    };

                    if (satapi_check_medium(cdrom.private_data) == 0) {
                        uint32_t max_lba, block_size;
                        if (satapi_read_capacity(cdrom.private_data, &max_lba, &block_size) == 0) {
                            cdrom.total_lba = (uint64_t)max_lba + 1;
                            cdrom.sector_size = block_size;
                        }
                    } else {
                        /* Label as No Medium */
                        int k = 0; const char* tag = " [NO MEDIUM]";
                        while(cdrom.name[k]) k++;
                        while(*tag) cdrom.name[k++] = *tag++;
                        cdrom.name[k] = '\0';
                    }

                    register_hardware_disk(cdrom);
                    vga_print("[AHCI] Port %d: ATAPI/SCSI Device Online.\n", p);
                }
                            }
                        }
                    }
                    return; /* Success: Controller found and initialized */
                }
                if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
}
