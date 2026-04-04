#include <stdint.h>

/* I/O Port Helpers */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * PIT Channel 2 (PC Speaker) based delay.
 * This does not require interrupts to be enabled.
 */
void pit_wait_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        /* Prepare PIT Channel 2 */
        uint8_t val = inb(0x61);
        /* bit 0: 1 (enable timer 2 gate), bit 1: 0 (disable speaker) */
        outb(0x61, (val & 0xFD) | 0x01);

        /* Set PIT to Mode 0, Channel 2, Lobyte/Hibyte */
        outb(0x43, 0xB0);

        /* 1193 ticks = 1ms at 1.193182 MHz frequency */
        outb(0x42, 0xA9); /* LSB */
        outb(0x42, 0x04); /* MSB */

        /* Wait for OUT bit (bit 5) of System Control Port B to go high */
        while (!(inb(0x61) & 0x20));
    }
}

static uint64_t system_ticks = 0;

void timer_handler(void) {
    system_ticks++;
}

uint64_t get_system_ticks(void) {
    return system_ticks;
}
