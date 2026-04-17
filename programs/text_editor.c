#define RSL_BINARY_MODE
#include <include/rsl.h>
#include <include/rtc64.h>
#include <include/vfs.h>
#include <include/config.h>
#include <stdint.h>
#include <stddef.h>

/* RSL Binary Header */
__attribute__((section(".header")))
rsl_header_t rsl_header = {
    .magic = {'R', 'S', 'L', '1'},
    .entry_offset = sizeof(rsl_header_t),
    .stack_size = 65536,
    .flags = 1 /* GUI-aware */
};

#define EDITOR_BUF_SIZE 2048
static char editor_buffer[EDITOR_BUF_SIZE];
static int editor_cursor = 0;

void editor_on_draw(int x, int y, int w, int h) {
    (void)w; (void)h;
    /* Use syscall for text drawing in standalone */
}

void editor_save(void) {
    rsl_print("[EDITOR] Saving is stubbed in standalone mode.\n");
}

void editor_on_event(rtc64_event_t ev) {
    (void)ev;
}

void wm_main(void) {
    for(int i=0; i<EDITOR_BUF_SIZE; i++) editor_buffer[i] = 0;
    rsl_print("[EDITOR] Text Editor Standalone Online.\n");
    while(1) {
        __asm__ volatile ("mov $1, %%rax; int $0x03" ::: "rax"); /* Syscall Yield */
    }
}

void _start(void) {
    wm_main();
}
