# OSx2 Limemade Hardware Interface

## 1. PCI Discovery
The kernel scans the PCI bus for relevant class/subclass pairs:
- **AHCI:** Class 0x01, Subclass 0x06
- **NVMe:** Class 0x01, Subclass 0x08
- **XHCI:** Class 0x0C, Subclass 0x03

## 2. AHCI/SATAPI
The AHCI driver implements a poll-only bootstrap during `EVENT_INIT`. It manually checks `PxTFD` and `PxIS` registers to handle disk I/O before interrupts are enabled.

## 3. NVMe Support
A PCIe MMIO discovery driver identifies NVMe controllers and prepares for Admin Queue mapping.

## 4. XHCI & BIOS Handover
The USB stack performs a BIOS handover protocol to take control of the XHCI controller registers during initialization.
