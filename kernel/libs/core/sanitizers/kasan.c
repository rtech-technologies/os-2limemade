#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Minimal KASAN Shadow Memory Implementation for RTECH OSx2 */
static uint8_t* kasan_shadow = NULL;
static bool kasan_enabled = false;

void vga_print(const char* fmt, ...);

void kasan_init(void* shadow_base) {
    kasan_shadow = (uint8_t*)shadow_base;
    kasan_enabled = true;
}

void __asan_load1(uintptr_t addr) { if (kasan_enabled && kasan_shadow[addr >> 3]) vga_print("KASAN: Read Access Violation at %x\n", addr); }
void __asan_load2(uintptr_t addr) { __asan_load1(addr); }
void __asan_load4(uintptr_t addr) { __asan_load1(addr); }
void __asan_load8(uintptr_t addr) { __asan_load1(addr); }

void __asan_store1(uintptr_t addr) { if (kasan_enabled && kasan_shadow[addr >> 3]) vga_print("KASAN: Write Access Violation at %x\n", addr); }
void __asan_store2(uintptr_t addr) { __asan_store1(addr); }
void __asan_store4(uintptr_t addr) { __asan_store1(addr); }
void __asan_store8(uintptr_t addr) { __asan_store1(addr); }

void __asan_handle_no_return(void) {}

/* KUBSAN Implementation */
struct source_location { const char *file; uint32_t line; uint32_t column; };

void __ubsan_handle_add_overflow(void *data, void *lhs, void *rhs) { (void)data; (void)lhs; (void)rhs; vga_print("UBSAN: Add Overflow\n"); }
void __ubsan_handle_sub_overflow(void *data, void *lhs, void *rhs) { (void)data; (void)lhs; (void)rhs; vga_print("UBSAN: Sub Overflow\n"); }
void __ubsan_handle_mul_overflow(void *data, void *lhs, void *rhs) { (void)data; (void)lhs; (void)rhs; vga_print("UBSAN: Mul Overflow\n"); }
void __ubsan_handle_type_mismatch_v1(void *data, void *ptr) { (void)data; (void)ptr; vga_print("UBSAN: Type Mismatch\n"); }
void __ubsan_handle_out_of_bounds(void *data, void *idx) { (void)data; (void)idx; vga_print("UBSAN: Out of Bounds\n"); }
