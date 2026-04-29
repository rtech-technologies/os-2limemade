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
void* malloc(size_t size);
void free(void* ptr);

void serial_write_str(const char* s);

static hba_mem_t* hba_base = NULL;
static void* port_clb_virt[32];
static void* port_fb_virt[32];
static void* port_ctba_virt[32];
static uint32_t g_registered_ports_mask = 0;

void* get_port_clb(int p) { return port_clb_virt[p]; }
void* get_port_ctba(int p) { return port_ctba_virt[p]; }
hba_mem_t* get_hba_base(void) { return hba_base; }

void serial_print_hex(const char* label, uint16_t val);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
void* bump_alloc(size_t size);
uint64_t vmm_get_phys(void* virt);
void serial_print(const char* fmt, ...);
void forensic_panic(const char* message, void* state);

void ahci_force_port_reset(int port_no);
#define panic(msg) forensic_panic(msg, NULL)
#define virtual_to_physical(virt) vmm_get_phys(virt)

void register_hardware_disk_from_port(int p);
void serial_print_hex32(const char* label, uint32_t val);

int ahci_wait_status(hba_port_t* port, uint32_t mask, uint32_t expected, uint32_t timeout_loops) {
    for (uint32_t i = 0; i < timeout_loops; i++) {
        /* Task File Error Status (Bit 30 of PxIS) */
        if (port->is & (1 << 30)) {
            serial_print_hex32("[AHCI] TFES Detected! TFD: ", port->tfd);
            return -1;
        }

        /* Logic: Check for SILICON-Ready bits in multiple registers */
        bool ci_clear = (port->ci & mask) == expected;
        bool tfd_ready = (port->tfd & 0x88) == 0; /* Not Busy and Not DRQ */
        bool is_fired = (port->is & mask);

        if (mask == (1 << 0)) { /* Command Issue Poll */
            if (ci_clear) return 0;
        } else if (is_fired) { /* Interrupt Status Poll */
            port->is = 0xFFFFFFFF;
            return 0;
        } else if (tfd_ready) { /* Task File Poll */
            return 0;
        }

        /* Non-Blocking: If scheduler is active, yield instead of hard stall */
        bool tasking_is_scanning(void);
        if (!tasking_is_scanning()) {
            sys_yield();
        } else {
            /* During INIT, we poll manually without full tasking */
            if (i % 1000 == 0) {
                pit_wait_ms(1);
                if (i % 10000 == 0) serial_write_str(".");
            }
        }
    }

    /* 🎯 Sentry Fix: Return error instead of panic to allow background recovery */
    return -1;
}

void ahci_port_stop(int p) {
    hba_port_t* port = &hba_base->ports[p];

    /* 1. Clear ST (bit 0) */
    port->cmd &= ~0x0001;

    /* 2. Wait until CR (bit 15) is cleared */
    int timeout = 500;
    while ((port->cmd & 0x8000) && timeout--) {
        pit_wait_ms(1);
    }

    /* 3. Clear FRE (bit 4) */
    port->cmd &= ~0x0010;

    /* 4. Wait until FR (bit 14) is cleared */
    timeout = 500;
    while ((port->cmd & 0x4000) && timeout--) {
        pit_wait_ms(1);
    }
}

void ahci_port_start(int p) {
    hba_port_t* port = &hba_base->ports[p];

    /* 1. Ensure the engine is stopped before configuring addresses */
    ahci_port_stop(p);

    /* 2. Register Physical Addresses (The Controller ignores HHDM) */
    uint64_t clb_phys = virtual_to_physical(port_clb_virt[p]);
    port->clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)(clb_phys >> 32);

    uint64_t fb_phys = virtual_to_physical(port_fb_virt[p]);
    port->fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)(fb_phys >> 32);

    /* 3. THE QEMU FIX: Force Interface to ACTIVE */
    /* We clear ICC (bits 28-31) and set it to 1 (Active) */
    /* We also set SUD (bit 1) to ensure the drive spins up */
    uint32_t cmd = port->cmd;
    cmd &= ~0xF0000000;      /* Clear ICC */
    cmd |= 0x10000000;       /* Set ICC to Active */
    cmd |= (1 << 1);         /* SUD: Spin Up Device */
    port->cmd = cmd;

    /* 4. Clear SError and Interrupts to prevent "Ghost" stalls */
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    /* 5. Enable FIS reception (FRE) */
    port->cmd |= (1 << 4);
    pit_wait_ms(10);

    /* 6. Start Engine (ST) */
    port->cmd |= (1 << 0);

    serial_print("[AHCI] Port %d: Engine Online. SSTS: 0x%x, TFD: 0x%x\n",
                  p, port->ssts, port->tfd);
}

void ahci_force_port_reset(int port_no) {
    hba_port_t* port = &hba_base->ports[port_no];

    ahci_port_stop(port_no);

    /* Perform COMRESET */
    port->sctl = (port->sctl & ~0x0F) | 0x01; /* DET=1 */
    pit_wait_ms(10);
    port->sctl = (port->sctl & ~0x0F) | 0x00; /* DET=0 */
    pit_wait_ms(1000000);

    /* Wait for the link to transition out of "Partial" (0x1) to "Active" (0x3) */
    int timeout = 500;
    while ((port->ssts & 0x0F) != 0x03 && timeout--) {
        pit_wait_ms(1);
    }

    /* Even if it's 0x113, ahci_port_start will now force it to Active */
    ahci_port_start(port_no);
}

void ahci_hardware_audit(int p) {
    if (!hba_base) return;
    if (!(hba_base->pi & (1 << p))) return;
    hba_port_t* port = &hba_base->ports[p];

    /* 🎯 Sentry Fix: Implement Background Recovery logic */
    /* If already registered, we don't need to re-initialize during background audit */
    if (g_registered_ports_mask & (1 << p)) return;

    /* Cooldown: Don't hammer the hardware (5 second interval) */
    static uint64_t last_audit_ticks[32] = {0};
    uint64_t now = get_system_ticks();
    if (now - last_audit_ticks[p] < 5000) return;
    last_audit_ticks[p] = now;

    /* SKIP: If port is "LIVE" (Busy or DRQ set), skip audit to avoid collision */
    if (port->tfd & 0x88) return;

    uint32_t ssts = port->ssts;
    if ((ssts & 0x0F) == 0x03) {
        /* Link is active but port is not registered. Attempt initialization. */
        ahci_port_start(p);
        /* If initialization succeeded and signature is valid, register the disk */
        if (port->sig == 0x00000101 || port->sig == 0xEB140101) {
            register_hardware_disk_from_port(p);
        }
    } else {
        /* Link is not active. Try forcing a reset to wake up the device. */
        ahci_force_port_reset(p);
    }
}

uint64_t get_hhdm_offset(void);

int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];

    if (port->sig == 0xEB140101) return -1;

    uint64_t phys_buffer = virtual_to_physical(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    /* CFL=5 (5*4=20 bytes), PRDTL=1 */
    cmdhdr->dw0 = 5 | (1 << 16);
    cmdhdr->prdbc = 0;

    uint64_t ctba_phys = virtual_to_physical(port_ctba_virt[p]);
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
    for (int i=0; i < (int)sizeof(hba_cmd_tbl_t); i++) ((uint8_t*)cmdtbl)[i] = 0;

    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    /* dw3: dbc=511 (for 1 sector per iteration in this simplified logic), i=1 */
    cmdtbl->prdt_entry[0].dw3 = ((count * 512 - 1) & 0x3FFFFF) | (1U << 31);

    uint32_t* fis = (uint32_t*)cmdtbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0x25 << 16); /* Type, Command(0x25=READ DMA EXT), C=1 */
    fis[1] = (lba & 0xFFFFFF) | (0x40 << 24); /* LBA Low 24 bits, Device=LBA Mode */
    fis[2] = (lba >> 24) & 0xFFFFFF;          /* LBA High 24 bits */
    fis[3] = count & 0xFFFF;                  /* Sector Count (16-bit) */

    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;

    port->ci = (1 << 0);

    /* Real Metal Poll: Wait for SILICON to clear CI bit */
    if (ahci_wait_status(port, (1 << 0), 0, 1000000) != 0) return -1;

    port->is = 0xFFFFFFFF;
    return 0;
}

int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];

    if (port->sig == 0xEB140101) return -1;

    uint64_t phys_buffer = virtual_to_physical(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    /* CFL=5, W=1, PRDTL=1 */
    cmdhdr->dw0 = 5 | (1 << 6) | (1 << 16);
    cmdhdr->prdbc = 0;

    uint64_t ctba_phys = virtual_to_physical(port_ctba_virt[p]);
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
    for (int i=0; i < (int)sizeof(hba_cmd_tbl_t); i++) ((uint8_t*)cmdtbl)[i] = 0;

    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    /* dw3: dbc, i=1 */
    cmdtbl->prdt_entry[0].dw3 = ((count * 512 - 1) & 0x3FFFFF) | (1U << 31);

    uint32_t* fis = (uint32_t*)cmdtbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0x35 << 16); /* Type, Command(0x35=WRITE DMA EXT), C=1 */
    fis[1] = (lba & 0xFFFFFF) | (0x40 << 24); /* LBA Low 24 bits, Device=LBA Mode */
    fis[2] = (lba >> 24) & 0xFFFFFF;          /* LBA High 24 bits */
    fis[3] = count & 0xFFFF;                  /* Sector Count (16-bit) */

    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;

    port->ci = (1 << 0);

    /* Real Metal Poll: Wait for SILICON to clear CI bit */
    if (ahci_wait_status(port, (1 << 0), 0, 1000000) != 0) return -1;

    port->is = 0xFFFFFFFF;
    return 0;
}

int ahci_mechanical_sync(int p) {
    if (!hba_base) return -1;
    hba_port_t* port = &hba_base->ports[p];

    /* Direct SATA Write bypass: Signature Stamp at LBA 0 */
    uint32_t stamp = 0xEFBEADDE;
    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)port_clb_virt[p];
    cmdhdr->dw0 = 5 | (1 << 6) | (1 << 16); /* CFL=5, W=1, PRDTL=1 */
    cmdhdr->prdbc = 0;

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)port_ctba_virt[p];
    for (int i=0; i < (int)sizeof(hba_cmd_tbl_t); i++) ((uint8_t*)cmdtbl)[i] = 0;

    /* Use static slab address for synchronous writes */
    static uint8_t sync_buf[512] __attribute__((aligned(16)));
    for(int i=0; i<512; i++) sync_buf[i] = 0;
    *(uint32_t*)sync_buf = stamp;

    uint64_t phys_buf = virtual_to_physical(sync_buf);
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buf & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buf >> 32);
    cmdtbl->prdt_entry[0].dw3 = (511 & 0x3FFFFF) | (1U << 31);

    uint32_t* fis = (uint32_t*)cmdtbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0x35 << 16); /* WRITE DMA EXT */
    fis[1] = (0 & 0xFFFFFF) | (0x40 << 24);   /* LBA 0 */
    fis[2] = 0;
    fis[3] = 1; /* 1 Sector */

    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;
    port->ci = (1 << 0);
    if (ahci_wait_status(port, (1 << 0), 0, 1000000) != 0) return -1;

    serial_print("[AHCI] Port %d: Mechanical Sync Success.\n", p);
    return 0;
}

int satapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
int satapi_eject(void* priv);
int satapi_identify(void* priv);
int satapi_check_medium(void* priv);
int satapi_read_capacity(void* priv, uint32_t* out_lba, uint32_t* out_ss);
int rtech_iso_init(int drive);

void register_hardware_disk_from_port(int p);

static void ahci_init_port_hw(int p) {
    void* slab_alloc_aligned(int id, size_t size, size_t align);
    /* CLB Alignment: AHCI Command Lists must be 1KB aligned, FB 256B aligned */
    port_clb_virt[p] = slab_alloc_aligned(0, 1024, 1024);
    port_fb_virt[p] = slab_alloc_aligned(0, 4096, 256);
    port_ctba_virt[p] = slab_alloc_aligned(0, 4096, 4096);

    if (!port_clb_virt[p] || !port_fb_virt[p] || !port_ctba_virt[p]) {
        serial_print("[AHCI] FATAL: Port %d Heap Allocation Failure.\n", p);
        return;
    }

    ahci_force_port_reset(p);

    /* Signature Delay: Wait for hardware to update registers after reset */
    pit_wait_ms(10);
}

void ahci_scan_remaining(void) {
    if (!hba_base) return;
    static bool scanned = false;
    if (scanned) return;
    scanned = true;

    serial_write_str("[AHCI] Performing background scan (Ports 5-31)...\n");
    for (int p = 5; p < 32; p++) {
        if (hba_base->pi & (1 << p)) {
            ahci_init_port_hw(p);
            if ((hba_base->ports[p].ssts & 0x0F) == 0x03) {
                register_hardware_disk_from_port(p);
            }
            sys_yield(); /* Background: Don't hog init time */
        }
    }
}

void register_hardware_disk_from_port(int p) {
    if (g_registered_ports_mask & (1 << p)) return;
    uint32_t sig = hba_base->ports[p].sig;
    if (sig == 0x00000101) { /* SATA */
        /* 🎯 Sentry Fix: Real Partition Discovery */
        uint8_t* sector = malloc(512);
        if (ahci_read_sectors((void*)(uint64_t)p, 1, 1, sector) == 0 && *(uint64_t*)sector == 0x5452415020494645ULL) {
            /* GPT Found. Scan for partitions. */
            if (ahci_read_sectors((void*)(uint64_t)p, 2, 1, sector) == 0) {
                for (int i = 0; i < 4; i++) {
                    uint8_t* entry = &sector[i * 128];
                    uint64_t start_lba = *(uint64_t*)&entry[32];
                    uint64_t end_lba = *(uint64_t*)&entry[40];
                    if (start_lba == 0) continue;

                    vdisk_node_t part = {
                        .sector_size = 512,
                        .total_lba = end_lba - start_lba + 1,
                        .partition_offset = start_lba,
                        .read_lba = ahci_read_sectors,
                        .write_lba = ahci_write_sectors,
                        .private_data = (void*)(uint64_t)p,
                        .is_atapi = false
                    };
                    /* Name: SATAx_Py */
                    int k = 0;
                    part.name[k++] = 'S'; part.name[k++] = 'A'; part.name[k++] = 'T'; part.name[k++] = 'A';
                    part.name[k++] = '0' + p; part.name[k++] = '_'; part.name[k++] = 'P'; part.name[k++] = '0' + i;
                    part.name[k++] = '\0';

                    register_hardware_disk(part);
                }
            }
        } else {
            /* Fallback: Register the whole disk or Sovereign default */
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
        }
        free(sector);
        g_registered_ports_mask |= (1 << p);
        serial_print("[AHCI] Port %d: SATA Hard Disk Online.\n", p);
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

        int satapi_identify(void* priv);
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
        g_registered_ports_mask |= (1 << p);
        serial_print("[AHCI] Port %d: ATAPI/SCSI Device Online.\n", p);

        /* ISO Discovery Handshake */
        rtech_iso_init(get_hw_disk_count() - 1);
    }
}

static bool ahci_ready = false;
bool ahci_is_ready(void) { return ahci_ready; }

void ahci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        if (hba_base != NULL) return; /* Shield: Already Initialized */

        serial_write_str("[INIT] Scanning PCI for SATA/AHCI controllers...\n");
        bool found = false;
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_config_read(bus, slot, func, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;
                uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                uint8_t base_class = (class_info >> 24) & 0xFF;
                uint8_t sub_class = (class_info >> 16) & 0xFF;

                if (base_class == 0x01 && sub_class == 0x06) {
                    vga_print("[AHCI] Found AHCI Controller at %d:%d:%d\n", bus, slot, func);
                    pci_enable_master(bus, slot, func);
                    uint32_t bar5 = pci_config_read(bus, slot, func, 0x24);
                    uint64_t hhdm = get_hhdm_offset();
                    hba_base = (hba_mem_t*)(hhdm + (uint64_t)(bar5 & 0xFFFFFFF0));
                    vga_print("[AHCI] Base Address: 0x%x, HBA Cap: 0x%x\n", (uint64_t)hba_base, hba_base->cap);

                    hba_base->ghc |= (1 << 31); /* AE */
                    hba_base->ghc |= (1 << 0);  /* HR */
                    int ghc_ms = 0;
                    while ((hba_base->ghc & (1 << 0)) && ghc_ms < 500) {
                        pit_wait_ms(1);
                        ghc_ms++;
                    }
                    hba_base->ghc |= (1 << 31); /* AE */

                    /* OSx2: Scan the first 5 ports on boot per Sovereign mandate (Single Threaded) */
                    for (int p = 0; p < 5; p++) {
                        if (hba_base->pi & (1 << p)) {
                            ahci_init_port_hw(p);
                            if ((hba_base->ports[p].ssts & 0x0F) == 0x03) {
                                register_hardware_disk_from_port(p);
                            }
                        }
                    }
                    ahci_ready = true;
                    found = true;
                    break;
                }
                if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
                }
                if (found) break;
            }
            if (found) break;
        }
        if (!found) {
            serial_write_str("[AHCI] No SATA/AHCI Controller found. Entering Degraded Mode.\n");
        }
    }
}
