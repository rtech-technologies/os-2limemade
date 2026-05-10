#include <stdint.h>
#include <stddef.h>

void generate_uuid_v4(char* out) {
    /* Simple PRNG for UUID v4 */
    static uint32_t seed = 0xDEADC0DE;
    const char* hex = "0123456789abcdef";

    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            out[i] = '-';
        } else if (i == 14) {
            out[i] = '4';
        } else {
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            out[i] = hex[seed % 16];
        }
    }
    out[36] = '\0';
}
