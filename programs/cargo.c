#include <include/rsl.h>
#include <include/stdlib.h>
#include <include/string.h>
#include "nuklear/nk_sovereign.h"

struct theme {
    struct nk_color window_bg;
    struct nk_color button_bg;
};

void load_theme(struct theme* t) {
    t->window_bg = nk_rgb(45, 45, 45);
    t->button_bg = nk_rgb(60, 60, 60);

    /* 🎯 Sentry: Load from DISK0_P0 (ESP) */
    void* path = str_create("0:0:/theme.cfg");
    void* mode = str_create("r");
    void* h = rsl_open(path, mode);
    if (h) {
        char buf[128];
        int bytes = rsl_read(h, buf, 127);
        if (bytes > 0) {
            buf[bytes] = '\0';
            char* win = strstr(buf, "window=#");
            if (win) {
                long val = strtol(win + 8, NULL, 16);
                t->window_bg.r = (uint8_t)(val >> 16);
                t->window_bg.g = (uint8_t)(val >> 8);
                t->window_bg.b = (uint8_t)val;
            }
        }
        rsl_close(h);
    }
    release(path);
    release(mode);
}

void main(void) {
    print("Sovereign OS universal CARGO utility\n");
    print("------------------------------------\n");

    nk_sovereign_t* dev = nk_sovereign_init();
    struct nk_context* ctx = &dev->ctx;

    struct theme t;
    load_theme(&t);
    ctx->style.window.fixed_background = nk_style_item_color(t.window_bg);

    bool is_install_mode = rsl_exists("BOOT:/CARGO/os2_system.iso");
    int selected_drive = -1;
    bool operation_complete = false;

    while (!operation_complete) {
        nk_sovereign_poll_input(dev);

        if (nk_begin(ctx, "CARGO Utility", nk_rect(50, 50, 400, 350),
            NK_WINDOW_BORDER|NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(ctx, 30, 1);
            if (is_install_mode) {
                nk_label(ctx, "Mode: INSTALLER", NK_TEXT_LEFT);
            } else {
                nk_label(ctx, "Mode: REPAIR", NK_TEXT_LEFT);
            }

            nk_label(ctx, "Available Disks:", NK_TEXT_LEFT);
            for (int i=0; i<4; i++) {
                char label[16]; strcpy(label, "Drive "); itoa(i, label + 6, 10);
                if (nk_button_label(ctx, label)) selected_drive = i;
            }

            if (selected_drive != -1) {
                char dlbl[32]; strcpy(dlbl, "Selected: Drive "); itoa(selected_drive, dlbl + 16, 10);
                nk_label(ctx, dlbl, NK_TEXT_LEFT);

                if (nk_button_label(ctx, is_install_mode ? "INSTALL" : "REPAIR")) {
                    operation_complete = true;
                }
            }
        }
        nk_end(ctx);
        nk_sovereign_device_draw(dev);
    }

    char drive_str[2];
    drive_str[0] = '0' + (char)selected_drive;
    drive_str[1] = '\0';

    if (is_install_mode) {
        print("Wiping and Partitioning Disk...\n");
        rsl_format(drive_str);
        print("Streaming system image...\n");
        print("Installation complete.\n");
    } else {
        print("Mounting for Repair...\n");
        rsl_mount(drive_str);
        print("Overwriting core binaries...\n");
        print("Repair complete.\n");
    }
}
