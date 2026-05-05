#include "pci.h"

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

static pci_driver_t pci_drivers[32];
static int pci_driver_count = 0;

void pci_register_driver(pci_driver_t driver) { if (pci_driver_count < 32) pci_drivers[pci_driver_count++] = driver; }

static int pci_match_id(pci_driver_t* driver, uint16_t vendor, uint16_t device) { for (int i = 0; driver->ids[i].vendor != 0; i++) if (driver->ids[i].vendor == vendor && driver->ids[i].device == device) return 1; return 0; }

void pci_scan_bus(void) { for (int b = 0; b < 256; b++) for (int s = 0; s < 32; s++) for (int f = 0; f < 8; f++) { uint32_t id = pci_config_read(b, s, f, 0); if ((id & 0xFFFF) != 0xFFFF) for (int d = 0; d < pci_driver_count; d++) if (pci_match_id(&pci_drivers[d], id & 0xFFFF, id >> 16)) pci_drivers[d].probe(b, s, f); } }
