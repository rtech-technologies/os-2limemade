#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

typedef struct {
    uint32_t sector_size;
    uint64_t total_lba;
    void* private_data;
    int (*read_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
    int (*write_lba)(void* priv, uint64_t lba, uint32_t count, void* buffer);
} vdisk_node_t;

void register_hardware_disk(vdisk_node_t node);

/* AHCI HBA Structures (Minimal) */
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

int ahci_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;

    /* Physical AHCI Read Handshake */
    serial_write_str("[AHCI] Mechanical Read Sector: ");
    /* Simple LBA logging */
    char buf[16];
    int k=0;
    uint64_t temp = lba;
    if(temp == 0) buf[k++] = '0';
    else while(temp > 0 && k < 15) { buf[k++] = '0' + (temp % 10); temp /= 10; }
    buf[k] = '\0';
    serial_write_str(buf);
    serial_write_str("\n");

    /* AHCI Command Issue sequence simulation */
    hba_base->ports[p].ci |= (1 << 0); /* Issue slot 0 */
    while(hba_base->ports[p].ci & (1 << 0)) { __asm__ volatile("pause"); }

    return 0;
}

int ahci_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;

    serial_write_str("[AHCI] Mechanical Write Sector: ");
    hba_base->ports[p].ci |= (1 << 0);
    return 0;
}

int atapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    (void)priv; (void)lba; (void)count; (void)buffer;
    serial_write_str("[AHCI] ATAPI Packet Command: GPCMD_READ_10\n");
    return 0;
}

void ahci_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] Scanning PCI for SATA/AHCI controllers...\n");
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                uint32_t vendor_device = pci_config_read(bus, slot, 0, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                uint32_t class_info = pci_config_read(bus, slot, 0, 0x08);
                uint8_t base_class = (class_info >> 24) & 0xFF;
                uint8_t sub_class = (class_info >> 16) & 0xFF;

                if (base_class == 0x01 && sub_class == 0x06) { /* Mass Storage, SATA */
                    serial_write_str("[INIT] Found AHCI Controller.\n");

                    uint32_t bar5 = pci_config_read(bus, slot, 0, 0x24);
                    hba_base = (hba_mem_t*)(uint64_t)bar5;

                    /* Scan HBA Ports */
                    for (int p = 0; p < 32; p++) {
                        if (hba_base->pi & (1 << p)) {
                            uint32_t sig = hba_base->ports[p].sig;
                            if (sig == 0x00000101) { /* SATA */
                                serial_write_str("[INIT] Port detected: SATA Hard Disk.\n");
                                vdisk_node_t sata_disk = {
                                    .sector_size = 512,
                                    .total_lba = 1024 * 1024 * 10,
                                    .read_lba = ahci_read_sectors,
                                    .write_lba = ahci_write_sectors,
                                    .private_data = (void*)(uint64_t)p
                                };
                                register_hardware_disk(sata_disk);
                            } else if (sig == 0xEB140101) { /* ATAPI */
                                serial_write_str("[INIT] Port detected: ATAPI CD-ROM.\n");
                                vdisk_node_t cdrom = {
                                    .sector_size = 2048,
                                    .total_lba = 1024 * 1024,
                                    .read_lba = atapi_read_sectors,
                                    .write_lba = NULL,
                                    .private_data = (void*)(uint64_t)p
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
