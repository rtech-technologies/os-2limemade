#ifndef AHCI_HW_H
#define AHCI_HW_H

#include <stdint.h>

/* AHCI HBA Structures (Physical) */
typedef struct {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
    uint32_t dw4;
} fis_reg_h2d_t;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dw3; /* dbc[21:0], reserved, i[31] */
} hba_prdt_entry_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    hba_prdt_entry_t prdt_entry[1];
} hba_cmd_tbl_t;

typedef struct {
    uint32_t dw0;   /* [04:00] CFL, [05] A, [06] W, [07] P, [08] R, [09] B, [10] C, [15:12] PMP, [31:16] PRDTL */
    uint32_t prdbc; /* [31:00] PRDBC (Byte Count Transferred) */
    uint32_t ctba;  /* [31:07] CTBA, [06:00] Reserved */
    uint32_t ctbau; /* [31:00] CTBAU */
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

#endif
