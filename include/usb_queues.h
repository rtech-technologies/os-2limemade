#ifndef USB_QUEUES_H
#define USB_QUEUES_H

#include <stdint.h>
#include <stdbool.h>

#define HID_QUEUE_SIZE 32

typedef struct {
    uint8_t data[8];
    int slot;
    int dci;
} hid_report_t;

typedef struct {
    hid_report_t reports[HID_QUEUE_SIZE];
    int head;
    int tail;
} hid_queue_t;

#endif
