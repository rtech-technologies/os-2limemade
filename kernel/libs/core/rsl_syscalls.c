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

static void* translate_user_ptr(void* ptr) {
    if (!ptr) return NULL;
    uint64_t addr = (uint64_t)ptr;

    /* If address is already in high memory (HHDM or Kernel), it's likely already translated or a kernel pointer */
    if (addr >= 0xFFFFFFFF80000000ULL || addr >= 0xFFFF800000000000ULL) return ptr;

    /* Standalone RSL Binary Support: Translate relative offset to Slab HHDM address */
    task_t* cur = get_current_task();
    if (cur && cur->is_transient) {
        void* slab_get_base(int id);
        uint8_t* base = (uint8_t*)slab_get_base(cur->slab_id);
        return (void*)(base + addr);
    }

    return ptr;
}

uint64_t rsl_syscall_handler(uint64_t id, uint64_t a1, uint64_t a2, uint64_t a3) {
    (void)a3;
    switch (id) {
        case 0: print((const char*)translate_user_ptr((void*)a1)); return 0;
        case 1: return (uint64_t)input((const char*)translate_user_ptr((void*)a1));
        case 2: set_color((color_t)a1, (color_t)a2); return 0;
        case 3: return (uint64_t)str_create((const char*)translate_user_ptr((void*)a1));
        case 4: release((void*)a1); return 0;
        case 5: return (uint64_t)str_match((void*)a1, (const char*)translate_user_ptr((void*)a2));
        case 6: return (uint64_t)str_concat((void*)a1, (void*)a2);
        case 7: return (uint64_t)str_to_cstr((void*)a1);
        case 8: return (uint64_t)str_is_empty((void*)a1);
        case 10: rsl_ls(translate_user_ptr((void*)a1)); return 0;
        case 11: rsl_cat(translate_user_ptr((void*)a1)); return 0;
        case 12: rsl_write(translate_user_ptr((void*)a1), translate_user_ptr((void*)a2)); return 0;
        case 13: rsl_mkdir(translate_user_ptr((void*)a1)); return 0;
        case 14: rsl_rmdir(translate_user_ptr((void*)a1)); return 0;
        case 15: return (uint64_t)rsl_exists(translate_user_ptr((void*)a1));
        case 16: rsl_mount(translate_user_ptr((void*)a1)); return 0;
        case 17: rsl_format(translate_user_ptr((void*)a1)); return 0;
        case 18: rsl_stamp(translate_user_ptr((void*)a1)); return 0;
        case 19: rsl_scan(); return 0;
        case 20: rsl_eject(translate_user_ptr((void*)a1)); return 0;
        case 100: rsl_execute_command((char*)translate_user_ptr((void*)a1)); return 0;
        case 200: { /* RTC64 GUI Update/Draw Cycle */
            void rtc64_update(void);
            void rtc64_draw_all(void);
            rtc64_update();
            rtc64_draw_all();
            return 0;
        }
        case 201: { /* RTC64 Get Surface - Placeholder */
            return 0;
        }
        case 300: return (uint64_t)cmdlets_execute_script((const char*)translate_user_ptr((void*)a1));
        case 301: return (uint64_t)cmdlets_execute_line((const char*)translate_user_ptr((void*)a1));
        default: return 0xFFFFFFFFFFFFFFFF;
    }
}
