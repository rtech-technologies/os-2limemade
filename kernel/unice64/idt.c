#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];
    descriptor->isr_low    = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs  = 0x08; /* Kernel code segment */
    descriptor->ist        = 0;
    descriptor->attributes = flags;
    descriptor->isr_mid    = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high   = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved   = 0;
}

extern void irq_timer_handler(void);
extern void exception_handler_stub(void);
extern void unice64_context_switch(void);
extern void rsl_syscall_stub(void);
extern void idt_load(idt_ptr_t* idt_ptr);

void idt_init(void) {
    idt_ptr.limit = (sizeof(idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    /* Fill IDT with exception stubs */
    for (int i = 0; i < 32; i++) {
        idt_set_descriptor(i, exception_handler_stub, 0x8E);
    }

    /* Register IRQ 32 (APIC Timer) */
    idt_set_descriptor(32, irq_timer_handler, 0x8E);

    /* Register software interrupt for sys_yield (0x81) */
    idt_set_descriptor(0x81, unice64_context_switch, 0xEE); /* Allow USermode access if needed */

    /* Register RSL Service Gate (0x03) */
    idt_set_descriptor(0x03, rsl_syscall_stub, 0x8E);

    idt_load(&idt_ptr);
}
