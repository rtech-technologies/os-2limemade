#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vdisk.h"

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
    uint32_t rsv1[11];
    uint32_t rsv2[3];
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
    uint8_t  rsv[0x100 - 0x24];
    hba_port_t ports[32];
} hba_mem_t;

/* External Symbols */
void vga_print(const char* fmt, ...);
uint64_t vmm_get_phys(void* virt);
void* get_port_clb(int p);
void* get_port_ctba(int p);
hba_mem_t* get_hba_base(void);

int atapi_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    hba_mem_t* hba_base = get_hba_base();
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];
    uint64_t phys_buffer = vmm_get_phys(buffer);

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)get_port_clb(p);
    cmdhdr->cfl = 5;
    cmdhdr->w = 0;
    cmdhdr->a = 1; /* ATAPI Bit */
    cmdhdr->p = 1; /* Prefetch */
    cmdhdr->prdtl = 1;

    uint64_t ctba_phys = vmm_get_phys(get_port_ctba(p));
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)get_port_ctba(p);
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dbc = (count * 2048) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    /* Construct ATA Command Packet FIS */
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmdtbl->cfis;
    for(int i=0; i<64; i++) cmdtbl->cfis[i] = 0;
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0xA0; /* ATA_CMD_PACKET */
    fis->featurel = 1;   /* DMA */
    fis->featureh = 0;
    fis->device = 0;     /* Bit 6 irrelevant for ATAPI Packet */

    /* Construct SCSI READ(10) Packet */
    for(int i=0; i<16; i++) cmdtbl->acmd[i] = 0;
    cmdtbl->acmd[0] = 0x28; /* READ(10) */
    cmdtbl->acmd[2] = (uint8_t)(lba >> 24);
    cmdtbl->acmd[3] = (uint8_t)(lba >> 16);
    cmdtbl->acmd[4] = (uint8_t)(lba >> 8);
    cmdtbl->acmd[5] = (uint8_t)lba;
    cmdtbl->acmd[7] = (uint8_t)(count >> 8);
    cmdtbl->acmd[8] = (uint8_t)count;

    /* Idle Wait: Drive must not be busy */
    int timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && timeout--) {
        __asm__ volatile ("pause");
    }

    port->ci = (1 << 0);
    timeout = 1000000;
    while ((port->ci & (1 << 0)) && timeout--) {
        if (port->tfd & (1 << 0)) {
            vga_print("[ATAPI] Port %d ERROR: TFD 0x%x\n", p, port->tfd);
            return -1;
        }
        __asm__ volatile ("pause");
    }
    if (timeout <= 0) {
        vga_print("[ATAPI] Port %d TIMEOUT\n", p);
        return -1;
    }
    return 0;
}

int atapi_eject(void* priv) {
    hba_mem_t* hba_base = get_hba_base();
    if (!hba_base) return -1;
    int p = (int)(uint64_t)priv;
    hba_port_t* port = &hba_base->ports[p];

    hba_cmd_header_t* cmdhdr = (hba_cmd_header_t*)get_port_clb(p);
    cmdhdr->cfl = 5;
    cmdhdr->w = 0;
    cmdhdr->a = 1;
    cmdhdr->prdtl = 0;

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)get_port_ctba(p);
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmdtbl->cfis;
    for(int i=0; i<64; i++) cmdtbl->cfis[i] = 0;
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0xA0;
    fis->featurel = 0; /* PIO for non-data commands */

    /* SCSI START STOP UNIT Packet */
    for(int i=0; i<16; i++) cmdtbl->acmd[i] = 0;
    cmdtbl->acmd[0] = 0x1B;
    cmdtbl->acmd[4] = 0x02; /* Eject bit */

    int timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && timeout--) {
        __asm__ volatile ("pause");
    }

    port->ci = (1 << 0);
    timeout = 1000000;
    while ((port->ci & (1 << 0)) && timeout--) {
        __asm__ volatile ("pause");
    }
    vga_print("[ATAPI] Port %d: Eject Signal Sent.\n", p);
    return 0;
}
