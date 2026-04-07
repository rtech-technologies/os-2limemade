#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} cpu_state_t;

void* get_xhci_base(void);
uint64_t get_hhdm_offset(void);
void serial_print_hex(const char* label, uint16_t val);

void tasking_set_scanning(bool scanning);

void forensic_panic(const char* message, cpu_state_t* state) {
    /* Sovereign Emergency Protocol: Synchronous Logging */
    tasking_set_scanning(true);
    __asm__ volatile ("cli");

    /* Critical Alert: Red on Black */
    set_color(LIGHT_RED, BLACK);

    /* Atomic Panic Report: One print call, one serial call */
    static char panic_buf[2048];
    int i = 0;

    /* 1. Build VGA Console Report */
    const char* vga_header = "\n!!! SOVEREIGN KERNEL PANIC !!!\nAutopsy Status: [ TERMINATED ]\nFailure Vector: ";
    while (*vga_header) panic_buf[i++] = *vga_header++;
    const char* m = message;
    while (*m && i < 1000) panic_buf[i++] = *m++;
    panic_buf[i++] = '\n';
    panic_buf[i++] = '\n';
    panic_buf[i] = '\0';
    print(panic_buf);

    /* 2. Build and Send Serial Report */
    i = 0;
    const char* ser_header = "\n[PANIC] !!! SOVEREIGN KERNEL EXCEPTION !!!\n[PANIC] Error Signature: ";
    while (*ser_header) panic_buf[i++] = *ser_header++;
    m = message;
    while (*m && i < 1000) panic_buf[i++] = *m++;
    panic_buf[i++] = '\n';
    if (state) {
        const char* autopsy_ok = "[AUTOPSY] CPU Register state capture successful.\n";
        while (*autopsy_ok) panic_buf[i++] = *autopsy_ok++;
    }

    void* xhci_ptr = get_xhci_base();
    if (xhci_ptr) {
        const char* usb_scan = "[AUTOPSY] Scanning USB registers...\n";
        while (*usb_scan) panic_buf[i++] = *usb_scan++;
        uint64_t hhdm = get_hhdm_offset();
        volatile uint32_t* op_regs = (uint32_t*)(hhdm + (uint64_t)xhci_ptr + 0x20);

        /* Inline Hex formatting to keep report atomic */
        const char* hex = "0123456789ABCDEF";
        const char* usbcmd_lbl = "XHCI_USBCMD: 0x";
        while (*usbcmd_lbl) panic_buf[i++] = *usbcmd_lbl++;
        uint32_t val = op_regs[0];
        for (int b = 7; b >= 0; b--) panic_buf[i++] = hex[(val >> (b * 4)) & 0xF];
        panic_buf[i++] = '\n';

        const char* usbsts_lbl = "XHCI_USBSTS: 0x";
        while (*usbsts_lbl) panic_buf[i++] = *usbsts_lbl++;
        val = op_regs[1];
        for (int b = 7; b >= 0; b--) panic_buf[i++] = hex[(val >> (b * 4)) & 0xF];
        panic_buf[i++] = '\n';
    } else {
        const char* no_xhci = "XHCI Controller Not Found.\n";
        while (*no_xhci) panic_buf[i++] = *no_xhci++;
    }

    panic_buf[i] = '\0';
    serial_write_str(panic_buf);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
