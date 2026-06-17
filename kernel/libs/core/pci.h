#ifndef PCI_H
#define PCI_H

#include <stdint.h>

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;
    void (*init)(uint8_t bus, uint8_t slot, uint8_t func);
} pci_driver_t;

void pci_register_driver(pci_driver_t driver);
void pci_scan_bus(void);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);

#endif
