#ifndef KSDK_USB_H
#define KSDK_USB_H
#include <ksdk/storage/xhci.h>
void xhci_irq_handler(void);
void* get_xhci_base(void);
#endif
