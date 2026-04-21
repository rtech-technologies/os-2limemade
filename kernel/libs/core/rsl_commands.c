#include <ksdk/core/ksdk.h>
void rsl_ls(void* path) { (void)path; }
void rsl_cat(void* path) { (void)path; }
void rsl_write(void* path, void* content) { (void)path; (void)content; }
void rsl_mkdir(void* path) { (void)path; }
void rsl_rmdir(void* path) { (void)path; }
void rsl_cd(void* path) { (void)path; }
bool rsl_exists(void* path) { (void)path; return false; }
void rsl_mount(void* path) { (void)path; }
void rsl_format(void* path) { (void)path; }
void rsl_stamp(void* path) { (void)path; }
bool rsl_safe_mode(void) { return false; }
void rsl_draw_rrif(void* path, int x, int y) { (void)path; (void)x; (void)y; }
void rsl_scan(void) { }
void rsl_eject(void* path) { (void)path; }
void rsl_settings(void) { }
void rsl_debug_dump(void) { }

/* Internal Glue */
void internal_fs_ls(void* path, void* priv) { (void)path; (void)priv; }
void internal_fs_cat(void* path, void* priv) { (void)path; (void)priv; }
void internal_fs_write(void* path, void* content, void* priv) { (void)path; (void)content; (void)priv; }
void internal_fs_mkdir(void* path, void* priv) { (void)path; (void)priv; }
void internal_fs_rmdir(void* path, void* priv) { (void)path; (void)priv; }
bool internal_fs_exists(void* path, void* priv) { (void)path; (void)priv; return false; }
void sovereign_service_orchestrator(void) { }
void vga_draw_mouse(void) { }
void draw_char_pixel(void) { }
void usb_main_task(void) { }
void usb_keyboard_task(void) { }
void usb_mouse_task(void) { }
void scheduler_force_task(void) { }
void register_transient_task(void) { }
void slab_grab_transient(void) { }
void slab_get_base(void) { }
void slab_release_transient(void) { }
