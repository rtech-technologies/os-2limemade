#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <include/vfs.h>
#include <include/rsl.h>

struct limine_memmap_response* get_memmap(void);

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// External Handshakes
extern struct limine_framebuffer_response* get_framebuffer(void);
uint64_t get_hhdm_offset(void);

// Cached Framebuffer Manifest
static struct {
    uint64_t address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    bool valid;
} fb_manifest = {0};

void panic_cache_fb(void) {
    struct limine_framebuffer_response* resp = get_framebuffer();
    if (resp && resp->framebuffer_count > 0) {
        struct limine_framebuffer* fb = resp->framebuffers[0];
        fb_manifest.address = (uint64_t)fb->address;
        fb_manifest.width = fb->width;
        fb_manifest.height = fb->height;
        fb_manifest.pitch = fb->pitch;
        fb_manifest.bpp = fb->bpp;
        fb_manifest.valid = true;
    }
}

// The CPU State Structure (Matches your context_switch.s / idt.c push order)
struct cpu_state {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

/* Minimal Embedded Font for Panic Recovery (8x8) */
static const uint8_t panic_font[128][8] = {
    [0x20] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [0x21] = { 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00 },
    [0x22] = { 0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [0x23] = { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00 },
    [0x24] = { 0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00 },
    [0x25] = { 0x00, 0x62, 0x66, 0x0C, 0x18, 0x33, 0x23, 0x00 },
    [0x26] = { 0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00 },
    [0x27] = { 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [0x28] = { 0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00 },
    [0x29] = { 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00 },
    [0x2A] = { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 },
    [0x2B] = { 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00 },
    [0x2C] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30 },
    [0x2D] = { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 },
    [0x2E] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00 },
    [0x2F] = { 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00 },
    [0x30] = { 0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00 },
    [0x31] = { 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 },
    [0x32] = { 0x3C, 0x66, 0x06, 0x3C, 0x60, 0x66, 0x7E, 0x00 },
    [0x33] = { 0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00 },
    [0x34] = { 0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00 },
    [0x35] = { 0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00 },
    [0x36] = { 0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    [0x37] = { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00 },
    [0x38] = { 0x3C, 0x66, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    [0x39] = { 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00 },
    [0x3A] = { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00 },
    [0x3B] = { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00 },
    [0x3C] = { 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00 },
    [0x3D] = { 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00 },
    [0x3E] = { 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00 },
    [0x3F] = { 0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00 },
    [0x40] = { 0x3C, 0x66, 0x6E, 0x6A, 0x60, 0x62, 0x3C, 0x00 },
    [0x41] = { 0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 },
    [0x42] = { 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00 },
    [0x43] = { 0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00 },
    [0x44] = { 0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00 },
    [0x45] = { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00 },
    [0x46] = { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00 },
    [0x47] = { 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00 },
    [0x48] = { 0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 },
    [0x49] = { 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 },
    [0x4A] = { 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00 },
    [0x4B] = { 0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00 },
    [0x4C] = { 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00 },
    [0x4D] = { 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00 },
    [0x4E] = { 0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00 },
    [0x4F] = { 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    [0x50] = { 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00 },
    [0x51] = { 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00 },
    [0x52] = { 0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00 },
    [0x53] = { 0x3C, 0x66, 0x30, 0x1C, 0x06, 0x66, 0x3C, 0x00 },
    [0x54] = { 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 },
    [0x55] = { 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    [0x56] = { 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 },
    [0x57] = { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 },
    [0x58] = { 0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00 },
    [0x59] = { 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x00 },
    [0x5A] = { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00 },
    [0x5B] = { 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00 },
    [0x5C] = { 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00 },
    [0x5D] = { 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00 },
    [0x5E] = { 0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [0x5F] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
    [0x60] = { 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [0x61] = { 0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00 },
    [0x62] = { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00 },
    [0x63] = { 0x00, 0x00, 0x3C, 0x60, 0x60, 0x66, 0x3C, 0x00 },
    [0x64] = { 0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00 },
    [0x65] = { 0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00 },
    [0x66] = { 0x1C, 0x30, 0x78, 0x30, 0x30, 0x30, 0x30, 0x00 },
    [0x67] = { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C },
    [0x68] = { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 },
    [0x69] = { 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00 },
    [0x6A] = { 0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x38 },
    [0x6B] = { 0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00 },
    [0x6C] = { 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 },
    [0x6D] = { 0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00 },
    [0x6E] = { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 },
    [0x6F] = { 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    [0x70] = { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60 },
    [0x71] = { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06 },
    [0x72] = { 0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00 },
    [0x73] = { 0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00 },
    [0x74] = { 0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00 },
    [0x75] = { 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00 },
    [0x76] = { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 },
    [0x77] = { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00 },
    [0x78] = { 0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00 },
    [0x79] = { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C },
    [0x7A] = { 0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00 },
    [0x7B] = { 0x0C, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0C, 0x00 },
    [0x7C] = { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 },
    [0x7D] = { 0x30, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x30, 0x00 },
    [0x7E] = { 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

void draw_string(int x, int y, const char* str, uint32_t color) {
    if (!fb_manifest.valid) return;

    for (int i = 0; str[i] != '\0'; i++) {
        uint8_t c = (uint8_t)str[i];
        if (c >= 128) continue;
        const uint8_t* glyph = panic_font[c];
        for (int gy = 0; gy < 8; gy++) {
            for (int gx = 0; gx < 8; gx++) {
                if (glyph[gy] & (1 << (7 - gx))) {
                    int px = x + (i * 8) + gx;
                    int py = y + gy;
                    if (px >= 0 && (uint64_t)px < fb_manifest.width && py >= 0 && (uint64_t)py < fb_manifest.height) {
                        uint32_t* pixel = (uint32_t*)(fb_manifest.address + py * fb_manifest.pitch + px * 4);
                        *pixel = color;
                    }
                }
            }
        }
    }
}

void int_to_hex(uint64_t val, char* out) {
    const char* hex_chars = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 16; i++) {
        out[2 + (15 - i)] = hex_chars[(val >> (i * 4)) & 0xF];
    }
    out[18] = '\0';
}

/**
 * @brief The Full Forensic Panic
 * @param message: A custom error string (can be NULL)
 * @param state: The CPU registers captured during the crash (can be NULL)
 */
void serial_write_str(const char* s);
void serial_write_char(char c);

void quartermaster_panic(const char* message, void* state) {
    // 1. Absolute Silence
    __asm__ volatile ("cli");

    /* Quartermaster: Forensic Reveal */
    extern void vga_force_verbose(void);
    vga_force_verbose();

    serial_write_str("\n\n!!! MECHANICAL FAILURE: SYSTEM HALTED !!!\n");
    if (message) {
        serial_write_str("DIAGNOSTIC MESSAGE: ");
        serial_write_str(message);
        serial_write_str("\n");
    }

    if (!fb_manifest.valid) {
        // Fallback to simple infinite loop if no cached manifest
        for (;;) { __asm__ volatile ("hlt"); }
    }

    uint32_t* fb_ptr = (uint32_t*)fb_manifest.address;
    struct cpu_state* regs = (struct cpu_state*)state;

    // 2. Paint the Sovereign Canvas RED
    // One step = one line: Mechanical clear
    for (uint64_t i = 0; i < (fb_manifest.pitch / 4) * fb_manifest.height; i++) {
        fb_ptr[i] = 0x00AA0000; // Deep Crimson
    }

    // 3. The Header
    draw_string(50, 50,  "********************************************", 0xFFFFFFFF);
    draw_string(50, 70,  "* MECHANICAL FAILURE: SYSTEM HALTED        *", 0xFFFFFFFF);
    draw_string(50, 90,  "* ALL CARGO HAS BEEN LOST OR CORRUPTED     *", 0xFFFFFFFF);
    draw_string(50, 110, "********************************************", 0xFFFFFFFF);

    // 4. Error Description
    draw_string(50, 150, "DIAGNOSTIC MESSAGE:", 0xFFFFFF00); // Yellow
    if (message) {
        draw_string(70, 170, message, 0xFFFFFFFF);
    } else {
        draw_string(70, 170, "CRITICAL EXCEPTION: NO MESSAGE PROVIDED", 0xFFFFFFFF);
    }

    // 5. Register Forensic Dump
    if (regs) {
        char buf[32];
        draw_string(50, 210, "FORENSIC REGISTER DUMP:", 0xFFFFFF00);
        serial_write_str("FORENSIC REGISTER DUMP:\n");

        int x1 = 70, x2 = 300, y = 230, step = 20;

        // Row 1
        int_to_hex(regs->rax, buf); draw_string(x1, y, "RAX:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->rbx, buf); draw_string(x2, y, "RBX:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 2
        int_to_hex(regs->rcx, buf); draw_string(x1, y, "RCX:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->rdx, buf); draw_string(x2, y, "RDX:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 3
        int_to_hex(regs->rsi, buf); draw_string(x1, y, "RSI:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->rdi, buf); draw_string(x2, y, "RDI:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 4
        int_to_hex(regs->rbp, buf); draw_string(x1, y, "RBP:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->rsp, buf); draw_string(x2, y, "RSP:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 5
        int_to_hex(regs->r8, buf);  draw_string(x1, y, "R8: ", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->r9, buf);  draw_string(x2, y, "R9: ", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 6
        int_to_hex(regs->r10, buf); draw_string(x1, y, "R10:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->r11, buf); draw_string(x2, y, "R11:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 7
        int_to_hex(regs->r12, buf); draw_string(x1, y, "R12:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->r13, buf); draw_string(x2, y, "R13:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 8
        int_to_hex(regs->r14, buf); draw_string(x1, y, "R14:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->r15, buf); draw_string(x2, y, "R15:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 9 (Segments)
        uint16_t ds, es, fs, gs;
        __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
        __asm__ volatile ("mov %%es, %0" : "=r"(es));
        __asm__ volatile ("mov %%fs, %0" : "=r"(fs));
        __asm__ volatile ("mov %%gs, %0" : "=r"(gs));
        int_to_hex(regs->cs, buf); draw_string(x1, y, "CS:", 0xAAAAAA); draw_string(x1+30, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->ss, buf); draw_string(x1+200, y, "SS:", 0xAAAAAA); draw_string(x1+230, y, buf, 0xFFFFFFFF);
        int_to_hex(ds, buf);       draw_string(x2, y, "DS:", 0xAAAAAA); draw_string(x2+30, y, buf, 0xFFFFFFFF);
        int_to_hex(es, buf);       draw_string(x2+200, y, "ES:", 0xAAAAAA); draw_string(x2+230, y, buf, 0xFFFFFFFF);
        y += step;
        int_to_hex(fs, buf);       draw_string(x1, y, "FS:", 0xAAAAAA); draw_string(x1+30, y, buf, 0xFFFFFFFF);
        int_to_hex(gs, buf);       draw_string(x1+200, y, "GS:", 0xAAAAAA); draw_string(x1+230, y, buf, 0xFFFFFFFF);
        y += step * 1.5;

        // Row 11 (Control)
        int_to_hex(regs->rip, buf); draw_string(x1, y, "RIP:", 0x55FF55); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->rflags, buf); draw_string(x2, y, "FLG:", 0x55FF55); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step;

        // Row 12 (Fault Info)
        int_to_hex(regs->interrupt_number, buf); draw_string(x1, y, "INT:", 0xFF5555); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        int_to_hex(regs->error_code, buf); draw_string(x2, y, "ERR:", 0xFF5555); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        y += step * 1.5;

        // Task Information
        #include <kernel/unice64/task.h>
        extern task_t* get_current_task(void);
        extern task_t* get_task_by_idx(int idx);
        task_t* curr = get_current_task();
        if (curr) {
            draw_string(x1, y, "CURRENT TASK:", 0xFFFFFF00);
            y += step;
            int_to_hex(curr->id, buf);      draw_string(x1, y, "ID: ", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
            int_to_hex(curr->uid, buf);     draw_string(x1+200, y, "UID:", 0xAAAAAA); draw_string(x1+240, y, buf, 0xFFFFFFFF);
            int_to_hex(curr->slab_id, buf); draw_string(x2+100, y, "SLAB:", 0xAAAAAA); draw_string(x2+140, y, buf, 0xFFFFFFFF);
            y += step * 1.5;
        }

        // System Inventory
        draw_string(x1, y, "SYSTEM INVENTORY:", 0xFFFFFF00);
        y += step;

        extern int get_hw_disk_count(void);
        int disks = get_hw_disk_count();
        draw_string(x1, y, "DISKS: ", 0xAAAAAA);
        buf[0] = (disks % 10) + '0'; buf[1] = '\0';
        draw_string(x1+60, y, buf, 0xFFFFFFFF);

        struct limine_memmap_response* mmap = get_memmap();
        if (mmap) {
            uint64_t usable = 0;
            for (uint64_t i = 0; i < mmap->entry_count; i++) {
                if (mmap->entries[i]->type == LIMINE_MEMMAP_USABLE) usable += mmap->entries[i]->length;
            }
            draw_string(x2, y, "RAM USABLE (MB):", 0xAAAAAA);
            int_to_hex(usable / 1024 / 1024, buf);
            draw_string(x2+120, y, buf, 0xFFFFFFFF);
        }
        y += step * 1.5;

        // Build Metadata
        draw_string(x1, y, "BUILD METADATA:", 0xFFFFFF00);
        y += step;
        draw_string(x1, y, "OSx2 LIMEMADE - MECHANICAL TRUTH - 2024-05-23", 0xAAAAAA);
        y += step;
        draw_string(x1, y, "GIT HASH: ffffffff8000d398-RELEASE-STABLE", 0xAAAAAA);

        serial_write_str("  RAX: "); int_to_hex(regs->rax, buf); serial_write_str(buf); serial_write_str("  RBX: "); int_to_hex(regs->rbx, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  RCX: "); int_to_hex(regs->rcx, buf); serial_write_str(buf); serial_write_str("  RDX: "); int_to_hex(regs->rdx, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  RSI: "); int_to_hex(regs->rsi, buf); serial_write_str(buf); serial_write_str("  RDI: "); int_to_hex(regs->rdi, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  RBP: "); int_to_hex(regs->rbp, buf); serial_write_str(buf); serial_write_str("  RSP: "); int_to_hex(regs->rsp, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  R8 : "); int_to_hex(regs->r8, buf);  serial_write_str(buf); serial_write_str("  R9 : "); int_to_hex(regs->r9, buf);  serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  R10: "); int_to_hex(regs->r10, buf); serial_write_str(buf); serial_write_str("  R11: "); int_to_hex(regs->r11, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  R12: "); int_to_hex(regs->r12, buf); serial_write_str(buf); serial_write_str("  R13: "); int_to_hex(regs->r13, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  R14: "); int_to_hex(regs->r14, buf); serial_write_str(buf); serial_write_str("  R15: "); int_to_hex(regs->r15, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  RIP: "); int_to_hex(regs->rip, buf); serial_write_str(buf); serial_write_str("  FLG: "); int_to_hex(regs->rflags, buf); serial_write_str(buf); serial_write_str("\n");
        serial_write_str("  INT: "); int_to_hex(regs->interrupt_number, buf); serial_write_str(buf); serial_write_str("  ERR: "); int_to_hex(regs->error_code, buf); serial_write_str(buf); serial_write_str("\n");

        // Machine-parsable JSON dump to serial
        serial_write_str("\n--- BEGIN PANIC JSON ---\n");
        serial_write_str("{\"panic\":{\"message\":\"");
        serial_write_str(message ? message : "NONE");
        serial_write_str("\",\"regs\":{\"rax\":\""); int_to_hex(regs->rax, buf); serial_write_str(buf);
        serial_write_str("\",\"rbx\":\""); int_to_hex(regs->rbx, buf); serial_write_str(buf);
        serial_write_str("\",\"rip\":\""); int_to_hex(regs->rip, buf); serial_write_str(buf);
        serial_write_str("\",\"rsp\":\""); int_to_hex(regs->rsp, buf); serial_write_str(buf);
        serial_write_str("\"},\"interrupt\":");
        int_to_hex(regs->interrupt_number, buf); serial_write_str(buf);
        serial_write_str(",\"error\":");
        int_to_hex(regs->error_code, buf); serial_write_str(buf);

        serial_write_str(",\"tasks\":[");
        for (int i = 0; i < get_task_count(); i++) {
            uint32_t tid, tslab;
            const char* tstate;
            get_task_info(i, &tid, &tstate, &tslab);
            serial_write_str("{\"id\":"); int_to_hex(tid, buf); serial_write_str(buf);
            serial_write_str(",\"state\":\""); serial_write_str(tstate);
            serial_write_str("\",\"slab\":"); int_to_hex(tslab, buf); serial_write_str(buf);

            task_t* t = get_task_by_idx(i);
            if (t) {
                serial_write_str(",\"last_rax\":"); int_to_hex(t->last_rax, buf); serial_write_str(buf);
            }

            serial_write_str("}");
            if (i < get_task_count() - 1) serial_write_str(",");
        }
        serial_write_str("]");

        /* AHCI State Audit */
        #include <include/ahci.h>
        extern hba_mem_t* get_hba_base(void);
        serial_write_str(",\"ahci\":[");
        hba_mem_t* hba = get_hba_base();
        if (hba) {
            for (int p = 0; p < 8; p++) {
                if (hba->pi & (1 << p)) {
                    serial_write_str("{\"port\":"); int_to_hex(p, buf); serial_write_str(buf);
                    serial_write_str(",\"ssts\":"); int_to_hex(hba->ports[p].ssts, buf); serial_write_str(buf);
                    serial_write_str(",\"tfd\":"); int_to_hex(hba->ports[p].tfd, buf); serial_write_str(buf);
                    serial_write_str("}");
                    if (p < 7) serial_write_str(",");
                }
            }
        }
        serial_write_str("]");

        /* Log Buffer Snapshot */
        serial_write_str(",\"log\":\"");
        extern void get_kernel_log_snapshot(char* out, uint32_t max);
        char log_snap[512];
        get_kernel_log_snapshot(log_snap, 511);
        for(int i=0; log_snap[i]; i++) {
            if (log_snap[i] == '\"') serial_write_str("\\\"");
            else if (log_snap[i] == '\n') serial_write_str("\\n");
            else serial_write_char(log_snap[i]);
        }
        serial_write_str("\"");

        serial_write_str("}}\n");
        serial_write_str("--- END PANIC JSON ---\n\n");

        // Attempt to write to disk if FS is mounted and not in safe mode
        extern bool vfs_is_safe_mode(void);
        if (!vfs_is_safe_mode()) {
            void* path = str_create("BOOT:/var/crash/panic.log");
            vfs_handle_internal_t* h = vfs_open(path, "w");
            if (h) {
                char crash_buf[1024];
                int len = 0;
                const char* head = "{\"panic\":{\"message\":\"";
                while(head[len]) { crash_buf[len] = head[len]; len++; }
                int ml = 0; while(message && message[ml] && len < 1000) { crash_buf[len++] = message[ml++]; }
                const char* mid = "\",\"rip\":\"";
                int midl = 0; while(mid[midl]) { crash_buf[len++] = mid[midl++]; }
                int_to_hex(regs->rip, buf);
                for(int i=0; i<18; i++) crash_buf[len++] = buf[i];
                const char* tail = "\"}}\n";
                int tl = 0; while(tail[tl]) { crash_buf[len++] = tail[tl++]; }
                vfs_write(h, crash_buf, len);
                vfs_close(h);
                release(h);
                serial_write_str("[SNAP] Crash log written to BOOT:/var/crash/panic.log\n");
            }
            release(path);
        }

        y += step;
        draw_string(x1, y, "CONTROL REGISTERS:", 0xFFFFFF00);
        serial_write_str("CONTROL REGISTERS:\n");
        y += step;

        uint64_t cr0, cr2, cr3, cr4;
        __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
        __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));

        int_to_hex(cr0, buf); draw_string(x1, y, "CR0:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        serial_write_str("  CR0: "); serial_write_str(buf);
        int_to_hex(cr2, buf); draw_string(x2, y, "CR2:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        serial_write_str("  CR2: "); serial_write_str(buf); serial_write_str("\n");
        y += step;
        int_to_hex(cr3, buf); draw_string(x1, y, "CR3:", 0xAAAAAA); draw_string(x1+40, y, buf, 0xFFFFFFFF);
        serial_write_str("  CR3: "); serial_write_str(buf);
        int_to_hex(cr4, buf); draw_string(x2, y, "CR4:", 0xAAAAAA); draw_string(x2+40, y, buf, 0xFFFFFFFF);
        serial_write_str("  CR4: "); serial_write_str(buf); serial_write_str("\n");
        y += step;

        if (regs->interrupt_number == 14) {
             draw_string(x1, y, "PF ANALYSIS: ", 0xFF5555);
             if (cr2 < 0x1000) draw_string(x1+100, y, "NULL POINTER DEREFERENCE", 0xFFFFFFFF);
             else draw_string(x1+100, y, "PAGE PRIVILEGE OR PROTECTION VIOLATION", 0xFFFFFFFF);
             y += step;
        }
        y += step * 0.5;

        // Stack Trace (Top 16 values)
        draw_string(x1, y, "STACK DUMP (RSP):", 0xFFFFFF00);
        serial_write_str("STACK DUMP (RSP):\n");
        y += step;
        uint64_t* stack = (uint64_t*)regs->rsp;
        for (int i = 0; i < 8; i++) {
            int_to_hex(stack[i], buf);
            draw_string(x1, y, buf, 0x55FFFF);
            serial_write_str("  "); serial_write_str(buf);
            int_to_hex(stack[i+8], buf);
            draw_string(x2, y, buf, 0x55FFFF);
            serial_write_str("  "); serial_write_str(buf); serial_write_str("\n");
            y += step;
        }

        y += step * 0.5;
        draw_string(x1, y, "CPU FLAGS STATE:", 0xFFFFFF00);
        y += step;

        /* Expanded Flags Decode */
        const char* flags_desc = (regs->rflags & (1 << 9)) ? "IF:1 (Interrupts ON)" : "IF:0 (Interrupts OFF)";
        draw_string(x1, y, flags_desc, 0xAAAAAA);
        y += step;
        flags_desc = (regs->rflags & (1 << 10)) ? "DF:1 (Direction DOWN)" : "DF:0 (Direction UP)";
        draw_string(x1, y, flags_desc, 0xAAAAAA);
        y += step;

        /* IDT/GDT Audit */
        uint64_t idtr[2], gdtr[2];
        __asm__ volatile ("sidt %0" : "=m"(idtr));
        __asm__ volatile ("sgdt %0" : "=m"(gdtr));
        draw_string(x1, y, "IDTR:", 0xAAAAAA); int_to_hex(idtr[1], buf); draw_string(x1+50, y, buf, 0xFFFFFFFF);
        serial_write_str("  IDTR: "); serial_write_str(buf);
        draw_string(x2, y, "GDTR:", 0xAAAAAA); int_to_hex(gdtr[1], buf); draw_string(x2+50, y, buf, 0xFFFFFFFF);
        serial_write_str("  GDTR: "); serial_write_str(buf); serial_write_str("\n");
        y += step;
    }

    // 6. Eternal Halt
    for (;;) { __asm__ volatile ("hlt"); }
}

void quartermaster_panic_reset(void) {
    /* Hard reset via Keyboard Controller (legacy but effective in QEMU) */
    serial_write_str("[SNAP] INITIATING MECHANICAL RESET\n");
    outb(0x64, 0xFE);
    for (;;) { __asm__ volatile ("hlt"); }
}
