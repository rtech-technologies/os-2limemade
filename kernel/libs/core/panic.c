#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <include/vfs.h>
#include <include/rsl.h>
#include <include/ahci.h>
#include <kernel/unice64/task.h>

static int panic_lock = 0;

struct limine_memmap_response* get_memmap(void);
extern struct limine_framebuffer_response* get_framebuffer(void);
extern uint64_t get_hhdm_offset(void);
extern void vga_force_verbose(void);
extern void unice64_kill_all_tasks(void);
extern int get_task_count(void);
extern task_t* get_task_by_idx(int idx);
extern task_t* get_current_task(void);
extern int get_hw_disk_count(void);
extern hba_mem_t* get_hba_base(void);
extern void get_kernel_log_snapshot(char* out, uint32_t max);
extern void serial_write_str(const char* s);
extern void serial_write_char(char c);

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

struct cpu_state {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

static const uint8_t panic_font[128][8] = {
    [0x20] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    [0x21] = { 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00 },
    [0x2A] = { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 },
    [0x2D] = { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 },
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
    [0x41] = { 0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 },
    [0x42] = { 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00 },
    [0x43] = { 0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00 },
    [0x44] = { 0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00 },
    [0x45] = { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00 },
    [0x46] = { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00 },
    [0x47] = { 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00 },
    [0x48] = { 0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 },
    [0x49] = { 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 },
    [0x4C] = { 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00 },
    [0x4D] = { 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00 },
    [0x4E] = { 0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00 },
    [0x52] = { 0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00 },
    [0x53] = { 0x3C, 0x66, 0x30, 0x1C, 0x06, 0x66, 0x3C, 0x00 },
    [0x54] = { 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 },
    [0x55] = { 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    [0x58] = { 0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00 },
};

void draw_char_panic(char c, int x, int y, uint32_t color) {
    if (!fb_manifest.valid || (uint8_t)c >= 128) return;
    const uint8_t* glyph = panic_font[(uint8_t)c];
    for (int gy = 0; gy < 8; gy++) {
        for (int gx = 0; gx < 8; gx++) {
            if (glyph[gy] & (1 << (7 - gx))) {
                int px = x + gx;
                int py = y + gy;
                if (px >= 0 && (uint64_t)px < fb_manifest.width && py >= 0 && (uint64_t)py < fb_manifest.height) {
                    uint32_t* pixel = (uint32_t*)(fb_manifest.address + py * fb_manifest.pitch + px * 4);
                    *pixel = color;
                }
            }
        }
    }
}

void draw_string_panic(int x, int y, const char* str, uint32_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char_panic(str[i], x + (i * 8), y, color);
    }
}

void int_to_hex_panic(uint64_t val, char* out) {
    const char* hex_chars = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; i++) out[2 + (15 - i)] = hex_chars[(val >> (i * 4)) & 0xF];
    out[18] = '\0';
}

void quartermaster_panic(const char* message, void* state) {
    __asm__ volatile ("cli");
    if (panic_lock) { for (;;) __asm__ volatile ("hlt"); }
    panic_lock = 1;

    unice64_kill_all_tasks();
    vga_force_verbose();

    serial_write_str("\n\n!!! MECHANICAL FAILURE: SYSTEM HALTED !!!\n");
    if (message) { serial_write_str(message); serial_write_str("\n"); }

    if (!fb_manifest.valid) panic_cache_fb();
    if (!fb_manifest.valid) { for (;;) __asm__ volatile ("hlt"); }

    uint32_t* fb_ptr = (uint32_t*)fb_manifest.address;
    struct cpu_state* regs = (struct cpu_state*)state;

    for (uint64_t i = 0; i < (fb_manifest.pitch / 4) * fb_manifest.height; i++) fb_ptr[i] = 0x00AA0000;

    draw_string_panic(50, 50,  "********************************************", 0xFFFFFFFF);
    draw_string_panic(50, 70,  "* MECHANICAL FAILURE: SYSTEM HALTED        *", 0xFFFFFFFF);
    draw_string_panic(50, 90,  "* ALL CARGO HAS BEEN LOST OR CORRUPTED     *", 0xFFFFFFFF);
    draw_string_panic(50, 110, "********************************************", 0xFFFFFFFF);

    if (message) draw_string_panic(50, 150, message, 0xFFFFFFFF);

    if (regs) {
        char buf[32];
        int_to_hex_panic(regs->rip, buf);
        serial_write_str("RIP: "); serial_write_str(buf); serial_write_str("\n");
        draw_string_panic(50, 200, "RIP: ", 0xAAAAAA);
        draw_string_panic(90, 200, buf, 0xFFFFFFFF);

        if (regs->rsp >= 0xFFFF800000000000ULL) {
            uint64_t* stack = (uint64_t*)regs->rsp;
            serial_write_str("STACK: ");
            for(int i=0; i<4; i++) {
                int_to_hex_panic(stack[i], buf);
                serial_write_str(buf); serial_write_str(" ");
            }
            serial_write_str("\n");
        }
    }

    serial_write_str("SYSTEM HALTED.\n");
    for (;;) { __asm__ volatile ("hlt"); }
}

void quartermaster_panic_reset(void) {
    serial_write_str("[SNAP] INITIATING MECHANICAL RESET\n");
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    for (;;) { __asm__ volatile ("hlt"); }
}
