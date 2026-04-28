#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <include/rsl.h>
#include <include/cmdlets.h>
#include <limine.h>
#include <include/string.h>
#include <include/stdlib.h>

void rsl_execute_command(char* line);
void* input(const char* prompt);
void set_color(color_t fg, color_t bg);
void* str_create(const char* cstr);
void release(void* ptr);
int slab_grab_transient(void);
void* slab_get_base(int id);
bool str_match(void* s, const char* p);
void* str_concat(void* s1, void* s2);
const char* str_to_cstr(void* s);
bool str_is_empty(void* s);

void rsl_ls(void* path);
void rsl_cat(void* path);
void rsl_write(void* path, void* content);
void rsl_mkdir(void* path);
void rsl_rmdir(void* path);
bool rsl_exists(void* path);
void rsl_mount(void* path);
void rsl_format(void* path);
void rsl_stamp(void* path);
void rsl_scan(void);
void rsl_eject(void* path);

static void* translate_user_ptr(void* ptr) {
    if (!ptr) return NULL;
    uint64_t addr = (uint64_t)ptr;
    task_t* cur = get_current_task();

    if (cur && cur->is_transient) {
        uint8_t* base = (uint8_t*)slab_get_base(cur->slab_id);
        uint64_t base_addr = (uint64_t)base;

        /* If absolute high-half address, verify slab boundary */
        if (addr >= 0xFFFF800000000000ULL) {
            if (addr >= base_addr && addr < base_addr + (4 * 1024 * 1024)) return ptr;
            return NULL; // 🎯 Sentry Fix: Block Slab Escape
        }

        /* If relative offset, verify it's within slab */
        if (addr < (4 * 1024 * 1024)) return (void*)(base + addr);
        return NULL; // 🎯 Sentry Fix: Block Out-of-Bounds Offset
    }

    /* Fallback for Kernel Tasks: Allow high memory directly */
    if (addr >= 0xFFFFFFFF80000000ULL || addr >= 0xFFFF800000000000ULL) return ptr;

    return ptr;
}

uint64_t rsl_syscall_handler(uint64_t id, uint64_t a1, uint64_t a2, uint64_t a3) {
    (void)a3;
    switch (id) {
        case 0: print((const char*)translate_user_ptr((void*)a1)); return 0;
        case 1: return (uint64_t)input((const char*)translate_user_ptr((void*)a1));
        case 2: set_color((color_t)a1, (color_t)a2); return 0;
        case 3: return (uint64_t)str_create((const char*)translate_user_ptr((void*)a1));
        case 4: release(translate_user_ptr((void*)a1)); return 0;
        case 5: return (uint64_t)str_match(translate_user_ptr((void*)a1), (const char*)translate_user_ptr((void*)a2));
        case 6: return (uint64_t)str_concat(translate_user_ptr((void*)a1), translate_user_ptr((void*)a2));
        case 7: return (uint64_t)str_to_cstr(translate_user_ptr((void*)a1));
        case 8: return (uint64_t)str_is_empty(translate_user_ptr((void*)a1));
        case 10: rsl_ls(translate_user_ptr((void*)a1)); return 0;
        case 11: rsl_cat(translate_user_ptr((void*)a1)); return 0;
        case 12: rsl_write(translate_user_ptr((void*)a1), translate_user_ptr((void*)a2)); return 0;
        case 13: rsl_mkdir(translate_user_ptr((void*)a1)); return 0;
        case 14: rsl_rmdir(translate_user_ptr((void*)a1)); return 0;
        case 15: return (uint64_t)rsl_exists(translate_user_ptr((void*)a1));
        case 16: rsl_mount(translate_user_ptr((void*)a1)); return 0;
        case 17: rsl_format(translate_user_ptr((void*)a1)); return 0;
        case 18: rsl_stamp(translate_user_ptr((void*)a1)); return 0;
        case 19: rsl_scan(); return 0;
        case 20: rsl_eject(translate_user_ptr((void*)a1)); return 0;
        case 100: rsl_execute_command((char*)translate_user_ptr((void*)a1)); return 0;
        case 200: { /* RTC64 GUI Update/Draw Cycle */
            void rtc64_update(void);
            void rtc64_draw_all(void);
            rtc64_update();
            rtc64_draw_all();
            return 0;
        }
        case 201: { /* Get New Transient Slab */
            int id = slab_grab_transient();
            if (id == -1) return 0;
            return (uint64_t)slab_get_base(id);
        }
        case 202: { /* Get Framebuffer Info */
            struct limine_framebuffer_response* get_framebuffer(void);
            struct limine_framebuffer_response* fb_resp = get_framebuffer();
            if (!fb_resp || fb_resp->framebuffer_count == 0) return 0;
            struct limine_framebuffer* fb = fb_resp->framebuffers[0];
            if (a1 == 0) return (uint64_t)fb->address;
            if (a1 == 1) return fb->width;
            if (a1 == 2) return fb->height;
            if (a1 == 3) return fb->pitch;
            return 0;
        }
        case 203: { /* DE_start - Prepare for GUI High Fidelity Mode */
            void vga_set_scale(int scale);
            vga_set_scale(1);
            void vga_clear(void);
            vga_clear();
            return 0;
        }
        case 204: { /* Get Mouse State */
            #include <include/mouse.h>
            mouse_state_t* ms = get_mouse_state();
            if (a1 == 0) return ms->x;
            if (a1 == 1) return ms->y;
            if (a1 == 2) {
                uint64_t buttons = 0;
                if (ms->left_button) buttons |= 1;
                if (ms->right_button) buttons |= 2;
                if (ms->middle_button) buttons |= 4;
                return buttons;
            }
            return 0;
        }
        case 210: { /* GUI Draw Line (x1:y1, x2:y2, color) */
            uint32_t x1 = (uint32_t)(a1 >> 32), y1 = (uint32_t)a1;
            uint32_t x2 = (uint32_t)(a2 >> 32), y2 = (uint32_t)a2;
            void rtc64_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
            rtc64_draw_line(x1, y1, x2, y2, (uint32_t)a3);
            return 0;
        }
        case 211: { /* ImGui Draw Triangle (p1, p2, p3, color) - Packed args */
            /* a1: x1:y1, a2: x2:y2, a3: x3:y3, a4 (not present, use global color or pack) */
            /* Simplified: ID 211 (p1:x|y, p2:x|y, p3:x|y) - uses current color */
            uint32_t x1 = (uint32_t)(a1 >> 32), y1 = (uint32_t)a1;
            uint32_t x2 = (uint32_t)(a2 >> 32), y2 = (uint32_t)a2;
            uint32_t x3 = (uint32_t)(a3 >> 32), y3 = (uint32_t)a3;
            void rtc64_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
            rtc64_draw_triangle(x1, y1, x2, y2, x3, y3, 0xFFFFFF);
            return 0;
        }
        case 212: { /* GUI Blit (x, y, w, h, data) */
            uint32_t x = (uint32_t)(a1 >> 32), y = (uint32_t)a1;
            uint32_t w = (uint32_t)(a2 >> 32), h = (uint32_t)a2;
            void rtc64_blit(int x, int y, int w, int h, uint32_t* data);
            rtc64_blit(x, y, w, h, (uint32_t*)translate_user_ptr((void*)a3));
            return 0;
        }
        case 213: { /* GUI Draw Char (c, px, py, fg, bg) */
            void draw_char_pixel(char c, int px, int py, uint32_t fg, uint32_t bg);
            draw_char_pixel((char)a1, (int)(a1 >> 32), (int)a2, (uint32_t)(a2 >> 32), (uint32_t)a3);
            return 0;
        }
        case 300: return (uint64_t)cmdlets_execute_script((const char*)translate_user_ptr((void*)a1));
        case 301: return (uint64_t)cmdlets_execute_line((const char*)translate_user_ptr((void*)a1));
        default: return 0xFFFFFFFFFFFFFFFF;
    }
}
