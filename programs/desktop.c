#include <include/rsl.h>

/* Minimal Desktop for New Users */

void tutorial(void) {
    print("\n[ WELCOME TO THE COMPUTER ]\n");
    print("This is a 'Computer'. It stores and processes information.\n");
    print("The screen shows you what the computer is thinking.\n\n");
    print("ESTATES (Places where things live):\n");
    print("1. [KERNEL] - The Master. It talks to the hardware (Disks/USB).\n");
    print("2. [TRUST ] - Safe Apps. They work for you using RSL.\n");
    print("3. [SCRIPT] - Experimental logic. Kept in a sandbox.\n\n");
    print("Type 'run' in the shell to start components.\n");
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    print("[DESKTOP] Sovereign Workspace Loaded.\n");
    tutorial();
    return 0;
}

void _start(void) {
    main(0, (void*)0);
    /* In RTECH, we don't return, we just yield or the task manager cleans us up */
    while(1) {
        __asm__ volatile ("int $3" : : "a"(2)); /* rsl_yield */
    }
}
