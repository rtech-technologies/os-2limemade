#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "nuklear/nuklear.h"
#include <include/rtc64.h>
#include <include/rsl.h>
#include <include/vfs.h>
#include <stddef.h>

void* malloc(size_t size);
void sys_yield(void);
void rsl_execute_stream(const char* path);
extern const char* g_build_manifest_hash;

static void rtc64_render_nk(struct nk_context* ctx) {
    const struct nk_command *cmd;
    nk_foreach(cmd, ctx) {
        switch (cmd->type) {
            case NK_COMMAND_RECT: {
                const struct nk_command_rect *r = (const struct nk_command_rect*)cmd;
                rtc64_draw_rect(r->x, r->y, r->w, r->h, *(uint32_t*)&r->color);
            } break;
            case NK_COMMAND_LINE: {
                const struct nk_command_line *l = (const struct nk_command_line*)cmd;
                rtc64_draw_line(l->begin.x, l->begin.y, l->end.x, l->end.y, *(uint32_t*)&l->color);
            } break;
            case NK_COMMAND_TEXT: {
                const struct nk_command_text *t = (const struct nk_command_text*)cmd;
                rtc64_draw_text((const char*)t->string, t->x, t->y, *(uint32_t*)&t->foreground);
            } break;
            default: break;
        }
    }
    nk_clear(ctx);
}

void desktop_main(void) {
    struct nk_context ctx;
    struct nk_user_font font;
    nk_init_fixed(&ctx, malloc(128*1024), 128*1024, &font);

    while (1) {
        rtc64_draw_rect(0, 0, 1024, 768, 0x004488);

        if (nk_begin(&ctx, "Sovereign Estates", nk_rect(50, 50, 250, 300), NK_WINDOW_TITLE|NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(&ctx, 30, 1);
            if (nk_button_label(&ctx, "Launch Estate: Shell")) { rsl_execute_stream("BOOT:/bin/shell.rsl"); }
            if (nk_button_label(&ctx, "Launch Estate: Cargo")) { rsl_execute_stream("BOOT:/bin/cargo.rsl"); }
        }
        nk_end(&ctx);

        if (nk_begin(&ctx, "System Info", nk_rect(600, 50, 400, 300), NK_WINDOW_TITLE|NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(&ctx, 20, 1);
            nk_label(&ctx, "OSx2 Limemade Build", NK_TEXT_LEFT);
            nk_label(&ctx, g_build_manifest_hash, NK_TEXT_LEFT);
            nk_label(&ctx, "Status: Verified", NK_TEXT_LEFT);
        }
        nk_end(&ctx);

        rtc64_render_nk(&ctx);

        extern void vga_compute_dirty(void);
        extern void vga_wait_vsync(void);
        extern void vga_flush_dirty(void);
        vga_compute_dirty();
        vga_wait_vsync();
        vga_flush_dirty();

        sys_yield();
    }
}
