#include <include/mouse.h>
#include <stdint.h>
#include <stdbool.h>

void serial_write_str(const char* s);

static mouse_state_t global_mouse = {0, 0, false, false, false, false};

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static int mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);

    /* Wait for ACK (0xFA) */
    uint32_t timeout = 10000;
    while(timeout--) {
        if (inb(0x64) & 1) {
            if (inb(0x60) == 0xFA) return 0;
        }
    }
    return -1;
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init(void) {
    serial_write_str("[MOUSE] Initializing PS/2 Mouse...\n");

    /* 1. Enable Auxiliary Device */
    mouse_wait(1);
    outb(0x64, 0xA8);

    /* 2. Check for existence: Reset command */
    if (mouse_write(0xFF) != 0) {
        serial_write_str("[MOUSE] No PS/2 Mouse detected.\n");
        global_mouse.active = false;
        return;
    }

    /* Consume ACK and Reset success byte (0xAA) */
    mouse_read();
    mouse_read();

    /* 3. Enable Interrupts in Controller Command Byte */
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    uint8_t status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    /* 4. Use Default Settings */
    mouse_write(0xF6);

    /* 5. Enable Packet Streaming */
    mouse_write(0xF4);

    global_mouse.active = true;
    serial_write_str("[MOUSE] PS/2 Mouse Online.\n");
}

void mouse_poll(void) {
    if (!global_mouse.active) return;

    /* Standard 3-byte PS/2 Packet */
    if ((inb(0x64) & 0x01) && (inb(0x64) & 0x20)) {
        uint8_t b1 = inb(0x60);
        uint8_t b2 = inb(0x60);
        uint8_t b3 = inb(0x60);

        global_mouse.left_button = (b1 & 0x01);
        global_mouse.right_button = (b1 & 0x02);
        global_mouse.middle_button = (b1 & 0x04);

        int8_t rel_x = (int8_t)b2;
        int8_t rel_y = (int8_t)b3;

        global_mouse.x += rel_x;
        global_mouse.y -= rel_y; /* Y is inverted in PS/2 */

        /* Bounds check would go here if we had a resolution, for now it "does nothing" */
    }
}

mouse_state_t* get_mouse_state(void) {
    return &global_mouse;
}
