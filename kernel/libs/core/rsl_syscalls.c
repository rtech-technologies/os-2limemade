#include <include/rsl.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);

/* Syscall Dispatcher Table */
uint64_t rsl_syscall_dispatcher(uint64_t id, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    (void)arg4;
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
            draw_char_pixel((char)arg1, (int)arg2, (int)arg3, (uint32_t)arg4, (uint32_t)0); // arg5 not used here yet
            break;
        }
        default:
            vga_print("[SYSCALL] Unknown RSL syscall ID: %d\n", id);
            break;
    }
    return 0;
}
