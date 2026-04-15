#include <include/rsl.h>
#include <include/ahci_hw.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void rsl_shutdown(void) {
    /* VGA Farewell */
    void vga_clear(void);
    void vga_set_cursor(int x, int y);
    vga_clear();
    set_color(WHITE, BLACK);
    vga_set_cursor(12, 33);
    print("Goodnight!!");

    /* Serial Debugging */
    serial_write_str("\n[OS] Initiating Sovereign Shutdown Protocol...\n");
    serial_write_str("[OS] Flushing SATA caches (Command 0xE7)...\n");

    /* 6-second mechanical safety delay */
    /* Wait for the HDD to physically spin down/park */
    void pit_wait_ms(uint32_t ms);
    pit_wait_ms(6000);

    serial_write_str("[OS] Powering off via ACPI...\n");

    /* OSx2: Clean Shutdown - Flush all AHCI caches */
    int ahci_flush_cache(int p);
    extern hba_mem_t* get_hba_base(void);
    hba_mem_t* hba = get_hba_base();
    if (hba) {
        for (int i = 0; i < 32; i++) {
            if (hba->pi & (1 << i)) ahci_flush_cache(i);
        }
    }

    /* ACPI Shutdown (QEMU/VirtualBox compatible) */
    outw(0x604, 0x2000);
    /* Alternative if above fails */
    outw(0xB004, 0x2000);

    set_color(LIGHT_GREEN, BLACK);
    vga_set_cursor(14, 25);
    print("you may now power off your pc");
    serial_write_str("\nyou may now power off your pc\n");

    for (;;) { __asm__ volatile ("hlt"); }
}
