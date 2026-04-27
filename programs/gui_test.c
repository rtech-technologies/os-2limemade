#include <include/rsl.h>
#include <programs/libc/libc.h>

int main(void) {
    print("Sovereign GUI Verification Utility\n");

    uint64_t addr = rsl_get_fb_info(0);
    uint64_t width = rsl_get_fb_info(1);
    uint64_t height = rsl_get_fb_info(2);
    uint64_t pitch = rsl_get_fb_info(3);

    print("FB Address: "); release(str_create("TODO: hex print")); print("\n");
    if (addr == 0) {
        print("Error: Could not retrieve framebuffer address.\n");
        return 1;
    }

    print("FB Resolution: ");
    /* Simple int to string for verification */
    if (width == 1024 && height == 768) print("1024x768 (Correct)\n");
    else print("Unexpected Resolution\n");

    print("Testing DE_start (Scale=1, Clear Screen)...\n");
    rsl_de_start();

    /* If we reach here, scaling is 1. Print should be smaller. */
    print("This text should be small (Scale 1).\n");

    while(1) {
        uint64_t mx = rsl_get_mouse(0);
        uint64_t my = rsl_get_mouse(1);
        uint64_t mb = rsl_get_mouse(2);

        if (mb & 1) {
            print("Left Click detected at ");
            /* exit test on click */
            break;
        }
    }

    return 0;
}
