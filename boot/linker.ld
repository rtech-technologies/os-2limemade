/* /boot/linker.ld - High-Half Sovereign Kernel */
ENTRY(_start)

PHDRS
{
    text    PT_LOAD FLAGS((1 << 0) | (1 << 2)); /* Execute + Read */
    rodata  PT_LOAD FLAGS((1 << 2));            /* Read Only */
    data    PT_LOAD FLAGS((1 << 1) | (1 << 2)); /* Write + Read */
}

SECTIONS
{
    /* High-Half Kernel: Linked at -2GB */
    . = 0xffffffff80000000;

    .text : ALIGN(4K) {
        *(.text .text.*)
        *(.programs)      /* Ensure the Shell is invited */
    } :text

    .rodata : ALIGN(4K) {
        *(.rodata .rodata.*)
    } :rodata

    .data : ALIGN(4K) {
        *(.limine_requests)
        *(.data .data.*)
    } :data

    .bss : ALIGN(4K) {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.note .note.*)
        *(.eh_frame)
    }
}
