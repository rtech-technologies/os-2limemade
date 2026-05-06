#include <kernel/libs/core/services.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

static uint32_t* virtual_buffer = NULL;
static uint32_t* back_buffer = NULL;
static size_t buffer_size = 0;

struct limine_framebuffer_response* get_framebuffer(void);
void* arc_alloc(size_t size);

void flusher_init(void) {
    struct limine_framebuffer_response* resp = get_framebuffer();
    if (!resp || resp->framebuffer_count == 0) return;
    struct limine_framebuffer* fb = resp->framebuffers[0];

    buffer_size = fb->pitch * fb->height;
    virtual_buffer = arc_alloc(buffer_size);
    back_buffer = arc_alloc(buffer_size);

    if (virtual_buffer && back_buffer) {
        for (size_t i = 0; i < buffer_size / 4; i++) {
            virtual_buffer[i] = 0x000000;
            back_buffer[i] = 0x000000;
        }
    }
}

void flusher_delta_move(void) {
    struct limine_framebuffer_response* resp = get_framebuffer();
    if (!resp || resp->framebuffer_count == 0 || !virtual_buffer || !back_buffer) return;
    struct limine_framebuffer* fb = resp->framebuffers[0];
    uint32_t* fb_ptr = (uint32_t*)fb->address;

    /* Operate on 64-pixel blocks (cacheline friendly) */
    for (size_t i = 0; i < buffer_size / 4; i += 64) {
        bool dirty = false;
        size_t end = i + 64;
        if (end > buffer_size / 4) end = buffer_size / 4;

        for (size_t j = i; j < end; j++) {
            if (virtual_buffer[j] != back_buffer[j]) {
                dirty = true;
                break;
            }
        }

        if (dirty) {
            for (size_t j = i; j < end; j++) {
                fb_ptr[j] = virtual_buffer[j];
                back_buffer[j] = virtual_buffer[j];
            }
        }
    }
}

uint32_t* get_virtual_buffer(void) {
    return virtual_buffer;
}
