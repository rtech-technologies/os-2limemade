#ifndef PANIC_H
#define PANIC_H

#include <stdbool.h>
#include <stddef.h>

void forensic_panic(const char* message, void* state);

#define PANIC_ON(condition, message) \
    do { \
        if (condition) { \
            forensic_panic(message, NULL); \
        } \
    } while (0)

#define PANIC_ON_ERR(error_code, message) \
    do { \
        if ((error_code) != 0) { \
            forensic_panic(message, NULL); \
        } \
    } while (0)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            forensic_panic("ASSERTION FAILED: " message, NULL); \
        } \
    } while (0)

#endif /* PANIC_H */
