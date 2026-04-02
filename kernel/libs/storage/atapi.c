#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>

/* Generic ATAPI / SCSI Constants */
#define SCSI_READ10 0x28
#define SCSI_START_STOP_UNIT 0x1B
#define SCSI_TEST_UNIT_READY 0x00
#define SCSI_READ_CAPACITY 0x25

/* External Symbols (Implemented by Transport Drivers like SATAPI) */
void vga_print(const char* fmt, ...);

/* The Generic ATAPI Logical Layer */

void atapi_build_read10_packet(uint8_t* packet, uint64_t lba, uint32_t count) {
    for(int i=0; i<12; i++) packet[i] = 0;
    packet[0] = SCSI_READ10;
    packet[2] = (lba >> 24) & 0xFF;
    packet[3] = (lba >> 16) & 0xFF;
    packet[4] = (lba >> 8) & 0xFF;
    packet[5] = lba & 0xFF;
    packet[7] = (count >> 8) & 0xFF;
    packet[8] = count & 0xFF;
}

void atapi_build_capacity_packet(uint8_t* packet) {
    for(int i=0; i<12; i++) packet[i] = 0;
    packet[0] = SCSI_READ_CAPACITY;
}

void atapi_build_tur_packet(uint8_t* packet) {
    for(int i=0; i<12; i++) packet[i] = 0;
    packet[0] = SCSI_TEST_UNIT_READY;
}

void atapi_build_eject_packet(uint8_t* packet) {
    for(int i=0; i<12; i++) packet[i] = 0;
    packet[0] = SCSI_START_STOP_UNIT;
    packet[4] = 0x02; /* Eject bit */
}
