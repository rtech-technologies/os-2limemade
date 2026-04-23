#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <include/rsl.h>
#include <include/cmdlets.h>

void rsl_execute_command(char* line);
void* input(const char* prompt);
void set_color(color_t fg, color_t bg);
void* str_create(const char* cstr);
void release(void* ptr);
bool str_match(void* s, const char* p);
void* str_concat(void* s1, void* s2);
const char* str_to_cstr(void* s);
bool str_is_empty(void* s);

void rsl_ls(void* path);
void rsl_cat(void* path);
void rsl_write(void* path, void* content);
void rsl_mkdir(void* path);
void rsl_rmdir(void* path);
bool rsl_exists(void* path);
void rsl_mount(void* path);
void rsl_format(void* path);
void rsl_stamp(void* path);
void rsl_scan(void);
void rsl_eject(void* path);

uint64_t rsl_syscall_handler(uint64_t id, uint64_t a1, uint64_t a2, uint64_t a3) {
    (void)a3;
    switch (id) {
        case 0: print((const char*)a1); return 0;
        case 1: return (uint64_t)input((const char*)a1);
        case 2: set_color((color_t)a1, (color_t)a2); return 0;
        case 3: return (uint64_t)str_create((const char*)a1);
        case 4: release((void*)a1); return 0;
        case 5: return (uint64_t)str_match((void*)a1, (const char*)a2);
        case 6: return (uint64_t)str_concat((void*)a1, (void*)a2);
        case 7: return (uint64_t)str_to_cstr((void*)a1);
        case 8: return (uint64_t)str_is_empty((void*)a1);
        case 10: rsl_ls((void*)a1); return 0;
        case 11: rsl_cat((void*)a1); return 0;
        case 12: rsl_write((void*)a1, (void*)a2); return 0;
        case 13: rsl_mkdir((void*)a1); return 0;
        case 14: rsl_rmdir((void*)a1); return 0;
        case 15: return (uint64_t)rsl_exists((void*)a1);
        case 16: rsl_mount((void*)a1); return 0;
        case 17: rsl_format((void*)a1); return 0;
        case 18: rsl_stamp((void*)a1); return 0;
        case 19: rsl_scan(); return 0;
        case 20: rsl_eject((void*)a1); return 0;
        case 100: rsl_execute_command((char*)a1); return 0;
        case 300: return (uint64_t)cmdlets_execute_script((const char*)a1);
        case 301: return (uint64_t)cmdlets_execute_line((const char*)a1);
        default: return 0xFFFFFFFFFFFFFFFF;
    }
}
