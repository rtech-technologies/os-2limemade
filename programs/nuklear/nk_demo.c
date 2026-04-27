#include "nk_sovereign.h"
#include <programs/libc/libc.h>

int main(void) {
    nk_sovereign_t* dev = nk_sovereign_init();
    struct nk_context* ctx = &dev->ctx;

    print("[Nuklear] Sovereign Demo Starting...\n");

    while (1) {
        nk_sovereign_poll_input(dev);

        if (nk_begin(ctx, "Sovereign UI", nk_rect(50, 50, 230, 250),
            NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
            NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
        {
            nk_layout_row_static(ctx, 30, 80, 1);
            if (nk_button_label(ctx, "Exit")) {
                break;
            }

            nk_layout_row_dynamic(ctx, 30, 2);
            nk_label(ctx, "Sovereign:", NK_TEXT_LEFT);
            nk_label(ctx, "Online", NK_TEXT_LEFT);

            static int check = 1;
            nk_checkbox_label(ctx, "Mechanical Truth", &check);
        }
        nk_end(ctx);

        nk_sovereign_device_draw(dev);
    }

    print("[Nuklear] Demo Exit.\n");
    return 0;
}
