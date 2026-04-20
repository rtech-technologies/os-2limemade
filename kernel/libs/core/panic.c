#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);
void serial_print_hex(const char* label, uint16_t val);
void* get_xhci_base(void);
uint64_t get_hhdm_offset(void);

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} cpu_state_t;

void forensic_panic(const char* message, cpu_state_t* state) {
    set_color(LIGHT_RED, BLACK);
    print("\n!!! SOVEREIGN KERNEL PANIC !!!\n");
    print(message); print("\n");
    serial_write_str("\n[PANIC] Error: "); serial_write_str(message); serial_write_str("\n");
    if (state) serial_write_str("[AUTOPSY] State captured.\n");
    for (;;) __asm__ volatile ("hlt");
}
