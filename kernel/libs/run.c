#include <include/rsl.h>
#include <kernel/libs/fatfs/ff.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);

#define MAX_VARS 64
typedef struct {
    char name[32];
    int64_t value;
} rsl_var_t;

static rsl_var_t namespace[MAX_VARS];
static int var_count = 0;

void rsl_set_var(const char* name, int64_t val) {
    for (int i = 0; i < var_count; i++) {
        if (str_match(str_create(name), namespace[i].name)) {
            namespace[i].value = val;
            return;
        }
    }
    if (var_count < MAX_VARS) {
        /* Simplified string copy */
        int i = 0;
        while(name[i] && i < 31) { namespace[var_count].name[i] = name[i]; i++; }
        namespace[var_count].name[i] = '\0';
        namespace[var_count].value = val;
        var_count++;
    }
}

int64_t rsl_get_var(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (str_match(str_create(name), namespace[i].name)) return namespace[i].value;
    }
    return 0;
}

void rsl_execute_stream(const char* path) {
    FIL fp;
    if (f_open(&fp, path, FA_READ) != FR_OK) {
        print("Error: Could not open RSL script.\n");
        return;
    }

    serial_write_str("[RSL] Executing script streamer: ");
    serial_write_str(path);
    serial_write_str("\n");

    char line[128];
    uint32_t br;
    while (f_read(&fp, line, sizeof(line)-1, &br) == FR_OK && br > 0) {
        line[br] = '\0';
        /* Logic Gate Evaluator and Streamer loop would go here */
        /* For now, we simulate execution by printing the bytecode stream */
        print(line);
    }

    f_close(&fp);
    print("\nRSL Execution Finished.\n");
}
