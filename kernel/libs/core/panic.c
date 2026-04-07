#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);

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
    /* Sovereign Emergency Protocol: Synchronous Logging */
    tasking_set_scanning(true);
    __asm__ volatile ("cli");

    /* Critical Alert: Red on Black */
    set_color(LIGHT_RED, BLACK);

    /* Atomic Panic Report: Comprehensive Hardware Autopsy */
    static char panic_buf[2048];
    int idx = 0;

    append_str(panic_buf, &idx, "\n!!! SOVEREIGN KERNEL PANIC !!!\n");
    append_str(panic_buf, &idx, "Autopsy Status: [ TERMINATED ]\n");
    append_str(panic_buf, &idx, "Failure Vector: ");
    append_str(panic_buf, &idx, message);
    append_str(panic_buf, &idx, "\n\n");

    /* System Telemetry */
    append_str(panic_buf, &idx, "[TELEMETRY] Ticks: ");
    append_hex64(panic_buf, &idx, get_system_ticks());

    task_t* current = get_current_task();
    if (current) {
        append_str(panic_buf, &idx, " | Active Task: 0x");
        const char* hex = "0123456789ABCDEF";
        uint32_t tid = current->id;
        for (int b = 7; b >= 0; b--) panic_buf[idx++] = hex[(tid >> (b * 4)) & 0xF];
    }

    uint64_t hhdm = get_hhdm_offset();
    append_str(panic_buf, &idx, " | HHDM: ");
    append_hex64(panic_buf, &idx, hhdm);
    append_str(panic_buf, &idx, "\n\n");

    /* CPU Core Dump */
    if (state) {
        append_str(panic_buf, &idx, "[CPU AUTOPSY]\n");
        append_str(panic_buf, &idx, "RAX: "); append_hex64(panic_buf, &idx, state->rax);
        append_str(panic_buf, &idx, " RBX: "); append_hex64(panic_buf, &idx, state->rbx);
        append_str(panic_buf, &idx, "\nRCX: "); append_hex64(panic_buf, &idx, state->rcx);
        append_str(panic_buf, &idx, " RDX: "); append_hex64(panic_buf, &idx, state->rdx);
        append_str(panic_buf, &idx, "\nRSI: "); append_hex64(panic_buf, &idx, state->rsi);
        append_str(panic_buf, &idx, " RDI: "); append_hex64(panic_buf, &idx, state->rdi);
        append_str(panic_buf, &idx, "\nRBP: "); append_hex64(panic_buf, &idx, state->rbp);
        append_str(panic_buf, &idx, " RSP: "); append_hex64(panic_buf, &idx, state->rsp);
        append_str(panic_buf, &idx, "\nRIP: "); append_hex64(panic_buf, &idx, state->rip);
        append_str(panic_buf, &idx, " RFLAGS: "); append_hex64(panic_buf, &idx, state->rflags);
        append_str(panic_buf, &idx, "\nCS : "); append_hex64(panic_buf, &idx, state->cs);
        append_str(panic_buf, &idx, " SS : "); append_hex64(panic_buf, &idx, state->ss);
        append_str(panic_buf, &idx, "\n\nR8 : "); append_hex64(panic_buf, &idx, state->r8);
        append_str(panic_buf, &idx, " R9 : "); append_hex64(panic_buf, &idx, state->r9);
        append_str(panic_buf, &idx, "\nR10: "); append_hex64(panic_buf, &idx, state->r10);
        append_str(panic_buf, &idx, " R11: "); append_hex64(panic_buf, &idx, state->r11);
        append_str(panic_buf, &idx, "\nR12: "); append_hex64(panic_buf, &idx, state->r12);
        append_str(panic_buf, &idx, " R13: "); append_hex64(panic_buf, &idx, state->r13);
        append_str(panic_buf, &idx, "\nR14: "); append_hex64(panic_buf, &idx, state->r14);
        append_str(panic_buf, &idx, " R15: "); append_hex64(panic_buf, &idx, state->r15);
        append_str(panic_buf, &idx, "\n\n");
    } else {
        append_str(panic_buf, &idx, "[CPU AUTOPSY] Register state not provided.\n\n");
    }

    /* Control Registers */
    uint64_t cr0, cr2, cr3, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));

    append_str(panic_buf, &idx, "[CONTROL REGISTERS]\n");
    append_str(panic_buf, &idx, "CR0: "); append_hex64(panic_buf, &idx, cr0);
    append_str(panic_buf, &idx, " CR2: "); append_hex64(panic_buf, &idx, cr2);
    append_str(panic_buf, &idx, "\nCR3: "); append_hex64(panic_buf, &idx, cr3);
    append_str(panic_buf, &idx, " CR4: "); append_hex64(panic_buf, &idx, cr4);
    append_str(panic_buf, &idx, "\n\n");

    panic_buf[idx] = '\0';

    /* Deliver Atomic Report to Hardware */
    print(panic_buf);
    serial_write_str(panic_buf);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
