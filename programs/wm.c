#include <include/rsl.h>

void wm_main(void) {
    set_color(LIGHT_CYAN, BLACK);
    print("\n--- RTECH SOVEREIGN WINDOW MANAGER (RTC64) ---\n");
    print("|  [X] Desktop                                                           |\n");
    while (1) {
        /* Syscall 20 logic would go here in the event loop */
        __asm__ volatile ("int $0x81");
    }
}
