# OSx2 Sovereign USB (xHCI) Architecture

This document describes the "Mechanical Truth" of the OSx2 USB stack, specifically focusing on the xHCI (eXtensible Host Controller Interface) implementation used for modern USB 3.0+ hardware and QEMU.

## 1. Core Files

*   **[`kernel/libs/storage/usb_xhci.c`](../kernel/libs/storage/usb_xhci.c):** The primary driver implementation. It handles PCI discovery, controller initialization, BIOS handover, and the device state machine.
*   **[`include/xhci.h`](../include/xhci.h):** Defines the XHCI register offsets, Transfer Request Block (TRB) structures, and Device/Input Context layouts.
*   **[`include/mouse.h`](../include/mouse.h):** The unified interface for routing HID Mouse data from XHCI Transfer Events to the Sovereign console.

## 2. The Initialization Ritual

The XHCI service is registered during `EVENT_INIT` and follows a strict sequence:

1.  **PCI Discovery:** Scans for Base Class `0x0C`, Sub Class `0x03`, and Prog IF `0x30`.
2.  **BIOS Handover:** Negotiates control from the BIOS using the XHCI Extended Capabilities (USB Legacy Support). It disables legacy PS/2 emulation to ensure direct OS control.
3.  **Controller Reset:** Stops the controller, triggers a hardware reset, and waits for the "Controller Not Ready" (CNR) bit to clear.
4.  **Ring Initialization:**
    *   **Command Ring:** Used by the OS to send commands to the controller.
    *   **Event Ring:** Used by the controller to post completions and hardware status changes.
5.  **Multitasking Online:** Once rings are active, the driver spawns the `xhci_monitor_task`.

## 3. The Device State Machine

To move a device from "plugged-in" to "operational," the `xhci_monitor_task` processes the following pipeline:

1.  **Port Reset:** Detects a connection via `PORTSC` bits and issues a Port Reset.
2.  **Enable Slot:** Sends an `ENABLE_SLOT` Command TRB. The controller returns a unique **Slot ID**.
3.  **Address Device:** Allocates a Device Context in the `DCBAAP` and sends an `ADDRESS_DEVICE` Command TRB using an Input Context to initialize the default control endpoint (EP0).
4.  **Endpoint Configuration:** For HID devices (Keyboard/Mouse), the driver configures Interrupt IN endpoints to receive asynchronous reports.

## 4. Input Processing (Active Spawner)

OSx2 uses an event-driven polling model for USB input:

*   **Transfer Events:** When a keyboard or mouse report is received, the controller posts a `TRB_TYPE_TRANSFER_EV` to the Event Ring.
*   **HID Router:** The `usb_keyboard_poll` function (called by `get_char`) parses these events, identifies the report type (Keyboard vs Mouse), and updates the system state.
*   **Non-Blocking:** If the scheduler is active, the driver yields (`sys_yield`) during hardware stalls, ensuring the system remains responsive.

## 5. QEMU Integration

USB support is enabled in the testing environment via the `Makefile`:
```makefile
-device qemu-xhci,id=xhci
-device usb-kbd,bus=xhci.0
-device usb-mouse,bus=xhci.0
```
