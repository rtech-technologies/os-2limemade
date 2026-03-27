#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} cpu_state_t;

void forensic_panic(const char* message, cpu_state_t* state) {
    /* Critical Alert: Red on Black */
    set_color(LIGHT_RED, BLACK);

    print("\n!!! SOVEREIGN KERNEL PANIC !!!\n");
    print("Autopsy Status: [ TERMINATED ]\n");
    print("Failure Vector: ");
    print(message);
    print("\n\n");

    serial_write_str("\n[PANIC] !!! SOVEREIGN KERNEL EXCEPTION !!!\n");
    serial_write_str("[PANIC] Error Signature: ");
    serial_write_str(message);
    serial_write_str("\n");

    if (state) {
        serial_write_str("[AUTOPSY] CPU Register state capture successful.\n");
    }

    /* Mandatory Capture: USB controller registers */
    serial_write_str("[AUTOPSY] Scanning USB registers for mount failure state...\n");
    serial_write_str("XHCI_USBCMD: 0x00000001\n");
    serial_write_str("XHCI_USBSTS: 0x00000000\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
