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

To move a device from "plugged-in" to "operational," the `xhci_monitor_task` (a persistent kernel task) processes the following pipeline:

1.  **Port Reset:** Detects a connection via `PORTSC` bits and issues a Port Reset.
2.  **Enable Slot:** Sends an `ENABLE_SLOT` Command TRB to the Command Ring. The controller returns a unique **Slot ID** via a Command Completion Event.
3.  **Addressing Handshake:**
    *   **Input Context:** Allocates a 2KB (64-byte aligned) `xhci_input_ctx_t`.
    *   **Slot Identity:** Populates the Slot Context with the root port number and context entries.
    *   **EP0 Pipe:** Allocates a dedicated Transfer Ring for Endpoint 0 (Control) and maps it in the EP0 Context.
    *   **Address Device:** Sends an `ADDRESS_DEVICE` Command TRB pointing to the Input Context.
4.  **Endpoint Configuration:** For HID devices (Keyboard/Mouse), the driver configures Interrupt IN endpoints to receive asynchronous reports.

## 4. Command Ring & Doorbells

Sending commands to the XHCI controller follows the "Active Spawner" model:
*   **Link TRBs:** The Command Ring uses Link TRBs (Type 6) to support seamless wraparound.
*   **Cycle Bits:** The OS toggles the Cycle Bit (DCS) on every TRB to signal ownership to the hardware.
*   **Doorbell Kick:** After pushing a TRB, the OS writes to Doorbell 0 (Host Controller) to wake the internal processing engine.

## 5. Input Processing (Active Spawner)

OSx2 uses an event-driven polling model for USB input:

*   **Transfer Events:** When a keyboard or mouse report is received, the controller posts a `TRB_TYPE_TRANSFER_EV` to the Event Ring.
*   **HID Router:** The `xhci_handle_events` function (called by the `usb_main_task`) parses these events, identifies the report type (Keyboard vs Mouse), and updates the system state. Keyboard events are pushed to the kernel's scancode buffer.
*   **Non-Blocking:** If the scheduler is active, the driver yields (`sys_yield`) during hardware stalls, ensuring the system remains responsive.

## 6. Telemetry Monitor

The Sovereign Telemetry Monitor (bottom row of the console) displays the state of the USB subsystem:

*   **`[M:X,Y]`**: The **'M'** stands for **Mouse**. It displays the real-time X and Y coordinates received from the HID Mouse or Tablet device. These values are updated whenever a HID report is processed by the xHCI event ring.
*   **`INPUT_WAIT`**: Indicates that the active task (e.g., the RSL Shell) is waiting for a USB keyboard or mouse event, causing the scheduler to yield to the background system task to maintain "Mechanical Truth" in power efficiency.

## 7. QEMU Integration

USB support is enabled in the testing environment via the `Makefile`:
```makefile
-device qemu-xhci,id=xhci
-device usb-kbd,bus=xhci.0
-device usb-mouse,bus=xhci.0
```
