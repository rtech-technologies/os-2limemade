#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>

/* AHCI / ATAPI Structures (Internal Mirror from ahci.c) */
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
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t rsv0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t devslp;
    volatile uint32_t rsv1[11];
    volatile uint32_t rsv2[3];
} hba_port_t;

typedef struct {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t bccc;
    volatile uint32_t bccd;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    uint8_t  rsv[0x100 - 0x24];
    hba_port_t ports[32];
} hba_mem_t;

/* External Symbols */
void vga_print(const char* fmt, ...);
void ahci_wait_status(hba_port_t* port, uint32_t mask, uint32_t expected, uint32_t timeout_loops);
uint64_t vmm_get_phys(void* virt);
void* get_port_clb(int p);
void* get_port_ctba(int p);
hba_mem_t* get_hba_base(void);

/* ATAPI Packet Builders (from atapi.c) */
void atapi_build_read10_packet(uint8_t* packet, uint64_t lba, uint32_t count);
void atapi_build_capacity_packet(uint8_t* packet);
void atapi_build_tur_packet(uint8_t* packet);
void atapi_build_eject_packet(uint8_t* packet);

int satapi_send_packet(int p, uint8_t* scsi_packet, void* buffer, uint32_t len, bool is_write) {
    hba_mem_t* hba_base = get_hba_base();
    if (!hba_base) return -1;
    hba_port_t* port = &hba_base->ports[p];

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)get_port_clb(p);
    cmdhdr->cfl = 5;
    cmdhdr->w = is_write ? 1 : 0;
    cmdhdr->a = 1;
    cmdhdr->p = 1;
    cmdhdr->prdtl = buffer ? 1 : 0;

    uint64_t ctba_phys = vmm_get_phys(get_port_ctba(p));
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)get_port_ctba(p);
    if (buffer) {
        uint64_t phys_buffer = vmm_get_phys(buffer);
        cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
        cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
        cmdtbl->prdt_entry[0].dbc = len - 1;
        cmdtbl->prdt_entry[0].i = 1;
    }

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmdtbl->cfis;
    for(int i=0; i<64; i++) cmdtbl->cfis[i] = 0;
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0xA0; /* ATA_CMD_PACKET */
    fis->featurel = buffer ? 1 : 0; /* DMA bit */

    for(int i=0; i<16; i++) cmdtbl->acmd[i] = 0;
    for(int i=0; i<12; i++) cmdtbl->acmd[i] = scsi_packet[i];

    /* Idle Wait: Wait for drive to be ready to receive command */
    ahci_wait_status(port, 0x80 | 0x08, 0, 1000000);

    port->ci = (1 << 0);
    ahci_wait_status(port, 1 << 0, 0, 1000000);

    /* Flush Interrupts */
    port->is = 0xFFFFFFFF;

    if (port->tfd & (1 << 0)) return -1;
    return 0;
}

int satapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    uint8_t packet[12];
    atapi_build_read10_packet(packet, lba, count);
    return satapi_send_packet((int)(uint64_t)priv, packet, buffer, count * 2048, false);
}

int satapi_check_medium(void* priv) {
    uint8_t packet[12];
    atapi_build_tur_packet(packet);
    return satapi_send_packet((int)(uint64_t)priv, packet, NULL, 0, false);
}

int satapi_identify(void* priv) {
    hba_mem_t* hba_base = get_hba_base();
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];

    uint16_t data[256];
    uint64_t phys_buffer = vmm_get_phys(data);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)get_port_clb(p);
    cmdhdr->cfl = 5;
    cmdhdr->w = 0;
    cmdhdr->a = 0; /* ATAPI bit is 0 for IDENTIFY PACKET command itself */
    cmdhdr->prdtl = 1;

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)get_port_ctba(p);
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = 512 - 1;
    cmdtbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmdtbl->cfis;
    for(int i=0; i<64; i++) cmdtbl->cfis[i] = 0;
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0xA1; /* IDENTIFY PACKET DEVICE */

    /* Idle Wait: Wait for drive to be ready to receive command */
    ahci_wait_status(port, 0x80 | 0x08, 0, 1000000);

    port->ci = (1 << 0);
    ahci_wait_status(port, 1 << 0, 0, 1000000);

    /* Flush Interrupts */
    port->is = 0xFFFFFFFF;

    if (port->tfd & (1 << 0)) return -1;
    return 0;
}

int satapi_read_capacity(void* priv, uint32_t* out_lba, uint32_t* out_ss) {
    uint8_t packet[12];
    atapi_build_capacity_packet(packet);
    uint32_t cap_data[2] = {0};
    if (satapi_send_packet((int)(uint64_t)priv, packet, cap_data, 8, false) == 0) {
        uint8_t* res = (uint8_t*)cap_data;
        if (out_lba) *out_lba = (res[0] << 24) | (res[1] << 16) | (res[2] << 8) | res[3];
        if (out_ss) *out_ss = (res[4] << 24) | (res[5] << 16) | (res[6] << 8) | res[7];
        return 0;
    }
    return -1;
}

int satapi_eject(void* priv) {
    uint8_t packet[12];
    atapi_build_eject_packet(packet);
    int res = satapi_send_packet((int)(uint64_t)priv, packet, NULL, 0, false);
    if (res == 0) vga_print("[SATAPI] Port %d: Eject Signal Sent.\n", (int)(uint64_t)priv);
    return res;
}
