#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>

/* Generic ATAPI / SCSI Constants */
#define SCSI_READ10 0x28
#define SCSI_START_STOP_UNIT 0x1B

/* External Symbols (Implemented by Transport Drivers like SATAPI) */
void vga_print(const char* fmt, ...);

/* The Generic ATAPI Logical Layer */
/* This file can be expanded with shared SCSI command builders */
