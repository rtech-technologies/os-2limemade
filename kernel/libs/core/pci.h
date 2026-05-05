#ifndef PCI_H
#define PCI_H

#include <stdint.h>

typedef struct { uint16_t vendor; uint16_t device; } pci_id_t;
typedef struct { const char* name; pci_id_t* ids; void (*probe)(uint8_t bus, uint8_t slot, uint8_t func); } pci_driver_t;

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void pci_enable_master(uint8_t bus, uint8_t slot, uint8_t func);
void pci_register_driver(pci_driver_t driver);
void pci_scan_bus(void);

#endif
