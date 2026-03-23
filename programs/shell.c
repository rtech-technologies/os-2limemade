#include <include/rsl.h>

void main(void) {
    color(10, 0); /* Emerald/Green on Black */
    print_cstr("Sovereign OS RSL Shell Initialized.\n");

    while (1) {
        print_cstr("rsl> ");
        managed_ptr_t input = readline();
        print_cstr("Command: ");
        print(input);
        print_cstr("\n");
        release(input);

        /* Command dispatch logic would go here */
        break; /* Exit for build verification purposes */
    }
}
