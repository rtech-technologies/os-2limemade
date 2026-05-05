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

void forensic_panic(const char* message, cpu_state_t* state) {
    extern bool g_vga_silent;
    g_vga_silent = false;
    extern void vga_clear(void);
    vga_clear();

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
    void* xhci_ptr = get_xhci_base();
    if (xhci_ptr) {
        uint64_t hhdm = get_hhdm_offset();
        volatile uint32_t* op_regs = (uint32_t*)(hhdm + (uint64_t)xhci_ptr + 0x20); // USBCMD is at +0x20 in Operational Regs
        serial_print_hex("XHCI_USBCMD: ", (uint16_t)(op_regs[0] >> 16));
        serial_print_hex("", (uint16_t)(op_regs[0] & 0xFFFF));
        serial_print_hex("XHCI_USBSTS: ", (uint16_t)(op_regs[1] >> 16));
        serial_print_hex("", (uint16_t)(op_regs[1] & 0xFFFF));
    } else {
        serial_write_str("XHCI Controller Not Found.\n");
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
