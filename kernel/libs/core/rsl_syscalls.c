#include <include/rsl.h>
#include <include/mouse.h>
#include <limine.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);

/* Syscall Dispatcher Table */
uint64_t rsl_syscall_dispatcher(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch(id) {
        case 0: /* print(const char*) */
            print((const char*)arg1);
            break;
        case 1: /* yield() */
            sys_yield();
            break;
        case 2: /* set_color(fg, bg) */
            set_color((color_t)arg1, (color_t)arg2);
            break;
        case 3: /* input(prompt) */
            return (uint64_t)input((const char*)arg1);
        case 4: /* str_to_cstr(str) */
            return (uint64_t)str_to_cstr((void*)arg1);
        case 5: /* release(ptr) */
            release((void*)arg1);
            break;
        case 6: /* rsl_shell_in() */
            rsl_shell_in();
            break;
        case 7: /* rsl_win_in() */
            rsl_win_in();
            break;
        case 10: /* draw_pixel(x, y, color) */
        {
            void draw_pixel(int x, int y, uint32_t color);
            draw_pixel((int)arg1, (int)arg2, (uint32_t)arg3);
            break;
        }
        case 11: /* draw_char_pixel(c, px, py, fg, bg) */
        {
            void draw_char_pixel(char c, int px, int py, uint32_t fg, uint32_t bg);
            draw_char_pixel((char)arg1, (int)arg2, (int)arg3, (uint32_t)arg4, (uint32_t)arg5);
            break;
        }
        case 12: /* draw_rect(x, y, w, h, color) */
        {
            void draw_rect(int x, int y, int w, int h, uint32_t color);
            draw_rect((int)arg1, (int)arg2, (int)arg3, (int)arg4, (uint32_t)arg5);
            break;
        }
        case 13: /* draw_circle(x, y, r, color) */
        {
            void draw_circle(int x, int y, int r, uint32_t color);
            draw_circle((int)arg1, (int)arg2, (int)arg3, (uint32_t)arg4);
            break;
        }
        case 14: /* draw_text_scaled(s, x, y, scale, color) */
        {
            void draw_text_scaled(const char* s, int x, int y, int scale, uint32_t color);
            draw_text_scaled((const char*)arg1, (int)arg2, (int)arg3, (int)arg4, (uint32_t)arg5);
            break;
        }
        case 15: /* draw_png(path, x, y) */
        {
            void rsl_draw_png(const char* path, int x, int y);
            rsl_draw_png((const char*)arg1, (int)arg2, (int)arg3);
            break;
        }
        case 20: /* GET_CUR_MPOS(axis) */
        {
            mouse_state_t* get_mouse_state(void);
            mouse_state_t* ms = get_mouse_state();
            if (arg1 == 0) return (uint64_t)ms->x;
            return (uint64_t)ms->y;
        }
        case 21: /* RSL_SURFACE_CREATE(w, h) */
        {
            /* Maps to slab allocation for now, in RSL apps usually Slab 4+ */
            void* malloc_ext(int id, size_t size);
            return (uint64_t)malloc_ext(0, (size_t)arg1 * (size_t)arg2 * 4);
        }
        case 22: /* RSL_SURFACE_PUSH(slab_addr) */
        {
            /* Proof of concept: blit entire 640x480 surface to GOP */
            /* In a real implementation we'd track surface dimensions */
            struct limine_framebuffer* get_global_fb(void);
            struct limine_framebuffer* fb = get_global_fb();
            if (fb) {
                void* memcpy(void* dest, const void* src, size_t n);
                memcpy(fb->address, (void*)arg1, 640 * 480 * 4);
            }
            break;
        }
        case 23: /* RSL_SURFACE_CLEAR(slab_addr, color) */
        {
            uint32_t* p = (uint32_t*)arg1;
            for(int k=0; k < 640*480; k++) p[k] = (uint32_t)arg2;
            break;
        }
        case 24: /* RSL_DRAW_VAL(x, y, value) */
        {
            void rsl_draw_val(int x, int y, int64_t val);
            rsl_draw_val((int)arg1, (int)arg2, (int64_t)arg3);
            break;
        }
        case 25: /* RSL_DRAW_HEX(x, y, value) */
        {
            void rsl_draw_hex(int x, int y, uint64_t val);
            rsl_draw_hex((int)arg1, (int)arg2, (uint64_t)arg3);
            break;
        }
        case 30: /* rsl_set_vga_silent(bool) */
        {
            void RSL_SET_VGA_SILENT(bool silent);
            RSL_SET_VGA_SILENT((bool)arg1);
            break;
        }
        case 31: /* vga_clear() */
        {
            void RSL_VGA_CLEAR(void);
            RSL_VGA_CLEAR();
            break;
        }
        case 32: /* RSL_WIN_GET_CHAR() - non-blocking */
        {
            char window_pop_char(void);
            return (uint64_t)window_pop_char();
        }
        default:
            vga_print("[SYSCALL] Unknown RSL syscall ID: %d\n", id);
            break;
    }
    return 0;
}
