#ifndef KSDK_PCI_H
#define KSDK_PCI_H
#include <stdint.h>
uint32_t pci_config_read(uint8_t b, uint8_t s, uint8_t f, uint8_t o);
void pci_enable_master(uint8_t b, uint8_t s, uint8_t f);
#endif
