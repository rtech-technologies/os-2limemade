#ifndef NK_SOVEREIGN_H
#define NK_SOVEREIGN_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_ASSERT(x)
#define STBRP_ASSERT(x)
#define STBTT_assert(x)

#include <programs/libc/libc.h>

#define NK_MEMCPY memcpy
#define NK_MEMSET memset
#define NK_STRTOL(s, e, b) 0
#define NK_DTOA(s, n)
#define NK_STRLEN strlen
#define STBRP_SORT qsort

#define NK_SIN(x) sin(x)
#define NK_COS(x) cos(x)
#define NK_ATAN2(y, x) 0
#define NK_STRTOD(s, e) 0
#define NK_INV_SQRT(x) (1.0/sqrt(x))

#define STBTT_malloc(x, u) malloc(x)
#define STBTT_free(x, u) free(x)
#define STBTT_strlen strlen
#define STBTT_memcpy memcpy
#define STBTT_memset memset
#define STBTT_pow pow
#define STBTT_sqrt sqrt
#define STBTT_fmod(x, y) ((x) - (int)((x) / (y)) * (y))
#define STBTT_cos cos
#define STBTT_acos(x) (1.570796 - (x))
#define STBTT_fabs fabs
#define STBTT_ifloor floor
#define STBTT_iceil ceil

#define NK_IMPLEMENTATION
#include <include/nuklear.h>
#include <include/rsl.h>

/* Sovereign ISA / RSL Runtime: Nuklear Backend */

typedef struct {
    struct nk_context ctx;
    struct nk_user_font font;
    struct nk_buffer cmds;
    uint32_t* fb;
    int width, height, pitch;
    int clip_x, clip_y, clip_w, clip_h;
    uint8_t memory[128 * 1024]; /* 128KB GUI State Pool */
} nk_sovereign_t;

static float nk_sovereign_font_width(nk_handle handle, float h, const char* text, int len) {
    (void)handle; (void)h; (void)text;
    return (float)len * 8.0f;
}

static void nk_sovereign_device_draw(nk_sovereign_t* dev) {
    const struct nk_command *cmd;
    nk_foreach(cmd, &dev->ctx) {
        switch (cmd->type) {
            case NK_COMMAND_SCISSOR: {
                const struct nk_command_scissor *s = (const struct nk_command_scissor*)cmd;
                dev->clip_x = s->x; dev->clip_y = s->y;
                dev->clip_w = s->w; dev->clip_h = s->h;
            } break;
            case NK_COMMAND_RECT_FILLED: {
                const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled*)cmd;
                uint32_t color = (r->color.r << 16) | (r->color.g << 8) | r->color.b;
                for (int y = 0; y < r->h; y++) {
                    for (int x = 0; x < r->w; x++) {
                        int px = r->x + x;
                        int py = r->y + y;
                        if (px >= dev->clip_x && px < dev->clip_x + dev->clip_w &&
                            py >= dev->clip_y && py < dev->clip_y + dev->clip_h) {
                            dev->fb[py * (dev->pitch/4) + px] = color;
                        }
                    }
                }
            } break;
            case NK_COMMAND_TEXT: {
                const struct nk_command_text *t = (const struct nk_command_text*)cmd;
                uint32_t fg = (t->foreground.r << 16) | (t->foreground.g << 8) | t->foreground.b;
                uint32_t bg = (t->background.r << 16) | (t->background.g << 8) | t->background.b;
                for (int i = 0; i < t->length; i++) {
                    /* Basic clipping for text */
                    if (t->x + (i*8) >= dev->clip_x && t->x + (i*8) < dev->clip_x + dev->clip_w &&
                        t->y >= dev->clip_y && t->y < dev->clip_y + dev->clip_h) {
                        rsl_draw_char(t->string[i], t->x + (i * 8), t->y, fg, bg);
                    }
                }
            } break;
            default: break;
        }
    }
    nk_clear(&dev->ctx);
}

static void nk_sovereign_poll_input(nk_sovereign_t* dev) {
    struct nk_context* ctx = &dev->ctx;
    nk_input_begin(ctx);

    int mx = (int)rsl_get_mouse(0);
    int my = (int)rsl_get_mouse(1);
    int mb = (int)rsl_get_mouse(2);

    nk_input_motion(ctx, mx, my);
    nk_input_button(ctx, NK_BUTTON_LEFT, mx, my, mb & 1);
    nk_input_button(ctx, NK_BUTTON_RIGHT, mx, my, mb & 2);

    nk_input_end(ctx);
}

nk_sovereign_t* nk_sovereign_init(void) {
    static nk_sovereign_t dev;
    rsl_de_start(); /* Reset scale and clear screen */

    dev.fb = (uint32_t*)rsl_get_fb_info(0);
    dev.width = (int)rsl_get_fb_info(1);
    dev.height = (int)rsl_get_fb_info(2);
    dev.pitch = (int)rsl_get_fb_info(3);

    dev.clip_x = 0;
    dev.clip_y = 0;
    dev.clip_w = dev.width;
    dev.clip_h = dev.height;

    dev.font.userdata = nk_handle_ptr(0);
    dev.font.height = 16;
    dev.font.width = nk_sovereign_font_width;

    nk_init_fixed(&dev.ctx, dev.memory, sizeof(dev.memory), &dev.font);
    return &dev;
}

#endif
