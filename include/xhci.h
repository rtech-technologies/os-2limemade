#ifndef XHCI_H
#define XHCI_H
#include <stdint.h>
typedef struct { uint32_t parameter[2]; uint32_t status; uint32_t control; } __attribute__((packed)) xhci_trb_t;
#endif
