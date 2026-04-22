#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <include/ahci_hw.h>

/* External Symbols */
void vga_print(const char* fmt, ...);
int ahci_wait_status(hba_port_t* port, uint32_t mask, uint32_t expected, uint32_t timeout_loops);
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
    /* CFL=5, A=1, W, P=1, PRDTL=1 */
    cmdhdr->dw0 = 5 | (1 << 5) | (is_write ? (1 << 6) : 0) | (1 << 7) | (buffer ? (1 << 16) : 0);
    cmdhdr->prdbc = 0;

    uint64_t ctba_phys = vmm_get_phys(get_port_ctba(p));
    cmdhdr->ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
    cmdhdr->ctbau = (uint32_t)(ctba_phys >> 32);

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)get_port_ctba(p);
    void* memset(void* s, int c, size_t n);
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));

    if (buffer) {
        uint64_t phys_buffer = vmm_get_phys(buffer);
        cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
        cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
        cmdtbl->prdt_entry[0].dw3 = ((len - 1) & 0x3FFFFF) | (1U << 31);
    }

    uint32_t* fis = (uint32_t*)cmdtbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0xA0 << 16) | ((buffer ? 1 : 0) << 24); /* Type, C, Command, FeatureL */

    for(int i=0; i<12; i++) cmdtbl->acmd[i] = scsi_packet[i];

    /* Wait for drive to be ready */
    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;

    port->ci = (1 << 0);
    if (ahci_wait_status(port, 1 << 0, 0, 1000000) != 0) return -1;

    port->is = 0xFFFFFFFF;
    if (port->tfd & 0x01) return -1; /* Error bit */
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
    cmdhdr->dw0 = 5 | (1 << 16);
    cmdhdr->prdbc = 0;

    hba_cmd_tbl_t* cmdtbl = (hba_cmd_tbl_t*)get_port_ctba(p);
    void* memset(void* s, int c, size_t n);
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buffer & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmdtbl->prdt_entry[0].dw3 = (511 & 0x3FFFFF) | (1U << 31);

    uint32_t* fis = (uint32_t*)cmdtbl->cfis;
    fis[0] = 0x27 | (1 << 15) | (0xA1 << 16); /* Type, C, Command (IDENTIFY PACKET) */

    if (ahci_wait_status(port, 0x88, 0, 1000000) != 0) return -1;
    port->ci = (1 << 0);
    if (ahci_wait_status(port, 1 << 0, 0, 1000000) != 0) return -1;
    port->is = 0xFFFFFFFF;
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
    return satapi_send_packet((int)(uint64_t)priv, packet, NULL, 0, false);
}
