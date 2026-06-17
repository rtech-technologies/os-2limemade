#include "pci.h"
#include <stdbool.h>
#include <stddef.h>

#define MAX_PCI_DRIVERS 32
static pci_driver_t drivers[MAX_PCI_DRIVERS];
static int driver_count = 0;

static bool bus_scanned[256][32][8] = {0};

void pci_register_driver(pci_driver_t driver) {
    if (driver_count < MAX_PCI_DRIVERS) {
        drivers[driver_count++] = driver;
    }
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    outl(0xCFC, val);
}

void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t command = pci_config_read(bus, slot, func, 0x04);
    command |= (1 << 1); /* Bit 1: Memory Space */
    command |= (1 << 2); /* Bit 2: Bus Master */
    pci_config_write(bus, slot, func, 0x04, command);
}

void pci_scan_bus(void) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t vendor_device = pci_config_read(bus, slot, func, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                if (bus_scanned[bus][slot][func]) continue;

                uint16_t vendor_id = vendor_device & 0xFFFF;
                uint16_t device_id = vendor_device >> 16;
                uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                uint8_t class_code = (class_info >> 24) & 0xFF;
                uint8_t subclass_code = (class_info >> 16) & 0xFF;
                uint8_t prog_if = (class_info >> 8) & 0xFF;

                for (int i = 0; i < driver_count; i++) {
                    bool match = true;
                    if (drivers[i].vendor_id != 0xFFFF && drivers[i].vendor_id != vendor_id) match = false;
                    if (drivers[i].device_id != 0xFFFF && drivers[i].device_id != device_id) match = false;
                    if (drivers[i].class_code != 0xFF && drivers[i].class_code != class_code) match = false;
                    if (drivers[i].subclass_code != 0xFF && drivers[i].subclass_code != subclass_code) match = false;
                    if (drivers[i].prog_if != 0xFF && drivers[i].prog_if != prog_if) match = false;

                    if (match && drivers[i].init) {
                        drivers[i].init(bus, slot, func);
                        bus_scanned[bus][slot][func] = true;
                    }
                }

                if (func == 0 && !(pci_config_read(bus, slot, 0, 0x0C) & 0x800000)) break;
            }
        }
    }
}
