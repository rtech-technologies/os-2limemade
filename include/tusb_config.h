#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#define CFG_TUSB_MCU                OPT_MCU_NONE
#define CFG_TUSB_OS                 OPT_OS_NONE

#define BOARD_TUH_RHPORT            0

/* USB Host Configuration */
#define CFG_TUH_ENABLED             1
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB                 1
#define CFG_TUH_DEVICE_MAX          4

/* Host Class Drivers */
#define CFG_TUH_HID                 4
#define CFG_TUH_MSC                 1
#define CFG_TUH_CDC                 0

#define CFG_TUSB_DEBUG              0

#endif
