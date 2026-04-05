#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/libs/storage/fatfs/ff.h>
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
    void* pstr = str_create(path);
    vfs_handle_t* h = vfs_open(pstr, "r");
    if (!h) {
        print("Error: Could not open RSL script.\n");
        release(pstr);
        return;
    }

    serial_write_str("[RSL] Executing script streamer: ");
    serial_write_str(path);
    serial_write_str("\n");

    char c;
    char line[256];
    int idx = 0;

    while (vfs_read(h, &c, 1) == 1) {
        if (c == '\n' || idx >= 255) {
            line[idx] = '\0';
            if (idx > 0) {
                /* Precise Execution: Single Line Processor */
                serial_write_str("[RSL] Executing: ");
                serial_write_str(line);
                serial_write_str("\n");

                void rsl_execute_command(char* line);
                rsl_execute_command(line);

                /* In a multitasking build, we would sovereign_yield() here */
                void sovereign_yield(void);
                sovereign_yield();
            }
            idx = 0;
        } else {
            line[idx++] = c;
        }
    }

    vfs_close(h);
    release(pstr);
    print("\nRSL Execution Finished.\n");
}
