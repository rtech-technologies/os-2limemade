#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void serial_write_str(const char* s);

#include <kernel/unice64/task.h>

void tasking_set_scanning(bool scanning);
uint64_t get_system_ticks(void);
uint64_t get_hhdm_offset(void);

int vsnprintf(char* str, size_t size, const char* format, va_list ap);

static void panic_printf(char* buf, size_t* pos, size_t max, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    *pos += vsnprintf(buf + *pos, max - *pos, fmt, ap);
    va_end(ap);
}

void forensic_panic(const char* message, cpu_context_t* state) {
    /* Sovereign Emergency Protocol: Synchronous Logging */
    tasking_set_scanning(true);
    __asm__ volatile ("cli");

    /* Critical Alert: Red on Black */
    set_color(LIGHT_RED, BLACK);

    /* Atomic Panic Report: Comprehensive Hardware Autopsy */
    static char panic_buf[2048];
    size_t pos = 0;
    size_t max = 2048;

    panic_printf(panic_buf, &pos, max, "\n!!! SOVEREIGN KERNEL PANIC !!!\n");
    panic_printf(panic_buf, &pos, max, "Autopsy Status: [ TERMINATED ]\n");
    panic_printf(panic_buf, &pos, max, "Failure Vector: %s\n\n", message);

    /* System Telemetry */
    panic_printf(panic_buf, &pos, max, "[TELEMETRY] Ticks: 0x%p", get_system_ticks());

    task_t* current = get_current_task();
    if (current) {
        panic_printf(panic_buf, &pos, max, " | Active Task: 0x%x", current->id);
    }

    panic_printf(panic_buf, &pos, max, " | HHDM: 0x%p\n\n", get_hhdm_offset());

    /* CPU Core Dump */
    if (state) {
        panic_printf(panic_buf, &pos, max, "[CPU AUTOPSY]\n");
        panic_printf(panic_buf, &pos, max, "RAX: 0x%p RBX: 0x%p\n", state->rax, state->rbx);
        panic_printf(panic_buf, &pos, max, "RCX: 0x%p RDX: 0x%p\n", state->rcx, state->rdx);
        panic_printf(panic_buf, &pos, max, "RSI: 0x%p RDI: 0x%p\n", state->rsi, state->rdi);
        panic_printf(panic_buf, &pos, max, "RBP: 0x%p RSP: 0x%p\n", state->rbp, state->rsp);
        panic_printf(panic_buf, &pos, max, "RIP: 0x%p RFLAGS: 0x%p\n", state->rip, state->rflags);
        panic_printf(panic_buf, &pos, max, "CS : 0x%x SS : 0x%x\n\n", (uint32_t)state->cs, (uint32_t)state->ss);
        panic_printf(panic_buf, &pos, max, "R8 : 0x%p R9 : 0x%p\n", state->r8, state->r9);
        panic_printf(panic_buf, &pos, max, "R10: 0x%p R11: 0x%p\n", state->r10, state->r11);
        panic_printf(panic_buf, &pos, max, "R12: 0x%p R13: 0x%p\n", state->r12, state->r13);
        panic_printf(panic_buf, &pos, max, "R14: 0x%p R15: 0x%p\n\n", state->r14, state->r15);
    } else {
        panic_printf(panic_buf, &pos, max, "[CPU AUTOPSY] Register state not provided.\n\n");
    }

    /* Control Registers */
    uint64_t cr0, cr2, cr3, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));

    panic_printf(panic_buf, &pos, max, "[CONTROL REGISTERS]\n");
    panic_printf(panic_buf, &pos, max, "CR0: 0x%p CR2: 0x%p\n", cr0, cr2);
    panic_printf(panic_buf, &pos, max, "CR3: 0x%p CR4: 0x%p\n\n", cr3, cr4);

    /* Deliver Atomic Report to Hardware - Use direct write bypass during panic */
    void vga_write_char(char c, uint8_t color_attr);
    void serial_write_str(const char* s);
    for (int k = 0; panic_buf[k]; k++) {
        vga_write_char(panic_buf[k], 0x4F); /* White on Red */
    }
    serial_write_str(panic_buf);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
