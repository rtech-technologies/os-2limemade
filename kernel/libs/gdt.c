#include <stdint.h>

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

static gdt_entry_t gdt[5];
static gdt_ptr_t   gdt_ptr;

void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low    = (base & 0xFFFF);
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = (limit & 0xFFFF);
    gdt[i].granularity = (limit >> 16) & 0x0F;
    gdt[i].granularity |= gran & 0xF0;
    gdt[i].access      = access;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    gdt_set_entry(0, 0, 0, 0, 0);                /* Null segment */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0); /* Kernel code (64-bit) */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xA0); /* Kernel data (64-bit) */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xA0); /* User code (64-bit) */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xA0); /* User data (64-bit) */

    __asm__ volatile (
        "lgdt %0\n\t"
        "push $0x08\n\t"
        "lea 1f(%%rip), %%rax\n\t"
        "push %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        : : "m"(gdt_ptr) : "rax"
    );
}
