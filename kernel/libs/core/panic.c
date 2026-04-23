#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

#include <kernel/unice64/task.h>

void tasking_set_scanning(bool scanning);
uint64_t get_system_ticks(void);
uint64_t get_hhdm_offset(void);

static void append_str(char* buf, int* idx, const char* s) {
    while (*s) buf[(*idx)++] = *s++;
}

static void append_hex64(char* buf, int* idx, uint64_t val) {
    const char* hex = "0123456789ABCDEF";
    append_str(buf, idx, "0x");
    for (int b = 15; b >= 0; b--) {
        buf[(*idx)++] = hex[(val >> (b * 4)) & 0xF];
    }
}

void forensic_panic(const char* message, cpu_context_t* state) {
    /* Sovereign Emergency Protocol */
    tasking_set_scanning(true);
    __asm__ volatile ("cli");

    static char panic_buf[2048];
    int idx = 0;

    append_str(panic_buf, &idx, "\n!!! SOVEREIGN KERNEL PANIC !!!\n");
    append_str(panic_buf, &idx, "Failure Vector: ");
    append_str(panic_buf, &idx, message);
    append_str(panic_buf, &idx, "\n\n");

    if (state) {
        append_str(panic_buf, &idx, "RIP: "); append_hex64(panic_buf, &idx, state->rip);
        append_str(panic_buf, &idx, " RSP: "); append_hex64(panic_buf, &idx, state->rsp);
        append_str(panic_buf, &idx, "\nRAX: "); append_hex64(panic_buf, &idx, state->rax);
        append_str(panic_buf, &idx, " RBX: "); append_hex64(panic_buf, &idx, state->rbx);
    }

    panic_buf[idx] = '\0';

    /* Deliver Report to Hardware */
    void vga_write_char(char c, uint8_t color_attr);
    void serial_write_str(const char* s);

    /* Deliver to both VGA and Serial via mirroring in vga_write_char */
    for (int k = 0; panic_buf[k]; k++) {
        vga_write_char(panic_buf[k], 0x4F); /* White on Red */
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
