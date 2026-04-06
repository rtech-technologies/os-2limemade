#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <include/ahci_hw.h>

void* malloc(size_t size);
void free(void* ptr);

void serial_write_str(const char* s);

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
void vga_print(const char* fmt, ...);
void forensic_panic(const char* message, void* state);

#define panic(msg) forensic_panic(msg, NULL)

int ahci_wait_status(hba_port_t* port, uint32_t mask, uint32_t expected, uint32_t timeout_loops) {
    (void)timeout_loops;
    uint32_t count = 0;
    while (count < 1000000) {
        /* 1. Task File Error bit check */
        if (port->tfd & (1 << 0)) return -1;

        /* 2. Manual Poll: Check PxIS, PxTFD, or PxCI based on mask/expected */
        if (port->is & mask) {
            port->is = 0xFFFFFFFF;
            return 0;
        }

        if (((port->tfd & mask) == expected) && ((port->ci & mask) == expected)) {
            return 0;
        }

        count++;
        __asm__ volatile ("pause");
    }

    panic("AHCI_POLL_TIMEOUT");
    return -1;
}

void ahci_port_start(int p) {
    hba_port_t* port = &hba_base->ports[p];
    int timeout = 10000000;
    while ((port->cmd & (1 << 15)) && timeout--) {
        __asm__ volatile ("pause");
    }

    /* Physical Registration: The Controller cannot see HHDM */
    uint64_t clb_phys = vmm_get_phys(port_clb_virt[p]);
    port->clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)(clb_phys >> 32);

    uint64_t fb_phys = vmm_get_phys(port_fb_virt[p]);
    port->fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)(fb_phys >> 32);

    port->cmd |= (1 << 4);
    port->cmd |= (1 << 0);
}

void pit_wait_ms(uint32_t ms);

void ahci_force_port_reset(int port_no) {
    hba_port_t* port = &hba_base->ports[port_no];
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;
    port->cmd &= ~0x0001;
    port->cmd &= ~0x0010;

    int engine_timeout = 10000000;
    while ((port->cmd & 0x8000 || port->cmd & 0x4000) && engine_timeout--) {
        __asm__ volatile ("pause");
    }

    port->sctl = (port->sctl & ~0x0F) | 0x301;
    pit_wait_ms(10);
    port->sctl = (port->sctl & ~0x0F) | 0x300;
    pit_wait_ms(50);

    int timeout = 10000000;
    while ((port->ssts & 0x0F) != 0x03 && timeout--) {
        __asm__ volatile ("pause");
    }

    if ((port->ssts & 0x0F) == 0x03) {
        vga_print("[AHCI] PORT %d: LINK ESTABLISHED\n", port_no);
        port->cmd |= 0x0010;
        ahci_port_start(port_no);
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
        ahci_port_start(p);
    } else {
        ahci_force_port_reset(p);
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
    if (ahci_wait_status(port, 0x80 | 0x01, 0, 1000000) != 0) return -1;

    port->ci = (1 << 0);
    if (ahci_wait_status(port, 1 << 0, 0, 1000000) != 0) return -1;

    /* Flush Interrupts */
    port->is = 0xFFFFFFFF;

    if (port->tfd & (1 << 0)) { /* ERR bit */
        vga_print("[AHCI] Port %d READ ERROR: TFD 0x%x\n", p, port->tfd);
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
    if (ahci_wait_status(port, 0x80 | 0x01, 0, 1000000) != 0) return -1;

    port->ci = (1 << 0);
    if (ahci_wait_status(port, 1 << 0, 0, 1000000) != 0) return -1;

    /* Flush Interrupts */
    port->is = 0xFFFFFFFF;

    if (port->tfd & (1 << 0)) { /* ERR bit */
        vga_print("[AHCI] Port %d WRITE ERROR: TFD 0x%x\n", p, port->tfd);
        return -1;
    }
    return 0;
}

int satapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
int satapi_eject(void* priv);
int satapi_identify(void* priv);
int satapi_check_medium(void* priv);
int satapi_read_capacity(void* priv, uint32_t* out_lba, uint32_t* out_ss);
int rtech_iso_init(int drive);

void ahci_scan_remaining(void) {
    if (!hba_base) return;
    vga_print("[AHCI] Performing extended scan (Ports 9-31)...\n");
    void* slab_alloc_aligned(int id, size_t size, size_t align);
    for (int p = 9; p < 32; p++) {
        if (hba_base->pi & (1 << p)) {
            /* CLB Alignment: AHCI Command Lists must be 1KB aligned */
            port_clb_virt[p] = slab_alloc_aligned(0, 1024, 1024);
            port_fb_virt[p] = slab_alloc_aligned(0, 4096, 4096);
            port_ctba_virt[p] = slab_alloc_aligned(0, 4096, 4096);

            if (!port_clb_virt[p] || !port_fb_virt[p] || !port_ctba_virt[p]) {
                vga_print("[AHCI] FATAL: Port %d Heap Allocation Failure.\n", p);
                continue;
            }

            /* Physical Registration: The Controller cannot see HHDM */
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

            ahci_force_port_reset(p);

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

                    /* ISO Discovery Handshake */
                    rtech_iso_init(get_hw_disk_count() - 1);
                }
            }
        }
        vga_print("[AHCI] No SATA/AHCI Controller found.\n");
    }
}

static bool ahci_ready = false;
bool ahci_is_ready(void) { return ahci_ready; }

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
                    int ghc_timeout = 10000000;
                    while ((hba_base->ghc & (1 << 0)) && ghc_timeout--) __asm__ volatile ("pause");
                    hba_base->ghc |= (1 << 31);

                    /* OSx2: Scan the first 9 ports on boot per Sovereign mandate */
                    void* slab_alloc_aligned(int id, size_t size, size_t align);
                    for (int p = 0; p < 9; p++) {
                        if (hba_base->pi & (1 << p)) {
                            /* CLB Alignment: AHCI Command Lists must be 1KB aligned */
                            port_clb_virt[p] = slab_alloc_aligned(0, 1024, 1024);
                            port_fb_virt[p] = slab_alloc_aligned(0, 4096, 4096);
                            port_ctba_virt[p] = slab_alloc_aligned(0, 4096, 4096);

                            if (!port_clb_virt[p] || !port_fb_virt[p] || !port_ctba_virt[p]) {
                                vga_print("[AHCI] FATAL: Port %d Heap Allocation Failure.\n", p);
                                continue;
                            }

                            /* Physical Registration: The Controller cannot see HHDM */
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

                            ahci_force_port_reset(p);

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

                    /* ISO Discovery Handshake */
                    rtech_iso_init(get_hw_disk_count() - 1);
                }
                            }
                        }
                    }
                    ahci_ready = true;
                    return; /* Success: Controller found and initialized */
                }
                if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
            }
        }
    }
}
