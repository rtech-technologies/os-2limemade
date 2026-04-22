#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * PIT Channel 2 based delay.
 * Safe for use during early boot before interrupts are enabled.
 */
void pit_wait_ms(uint32_t ms) {
    if (ms == 0) return;

    for (uint32_t i = 0; i < ms; i++) {
        uint8_t val = inb(0x61);
        /* Enable PIT Channel 2 gate, disable speaker */
        outb(0x61, (val & 0xFD) | 0x01);

        /* Set PIT to Mode 0 (Interrupt on Terminal Count), Channel 2, LSB/MSB */
        outb(0x43, 0xB0);

        /* 1193182 Hz / 1000 = 1193 (0x04A9) ticks per millisecond */
        outb(0x42, 0xA9); /* LSB */
        outb(0x42, 0x04); /* MSB */

        /* Wait for OUT bit (bit 5) of System Control Port B to go high */
        volatile int safety = 1000000;
        while (!(inb(0x61) & 0x20) && safety--) {
            __asm__ volatile ("pause");
        }
    }
}

static uint64_t system_ticks = 0;
void timer_handler(void) { system_ticks++; }
uint64_t get_system_ticks(void) { return system_ticks; }
