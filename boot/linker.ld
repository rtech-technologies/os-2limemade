/* /boot/linker.ld - The Sovereign Identity Fix */
ENTRY(_start)

SECTIONS
{
    /* Start at 2MB - No more High-Half Ghosting */
    . = 2M;

    .text : ALIGN(4K) {
        *(.text .text.*)
        *(.programs)      /* Ensure the Shell is invited */
    }

    .rodata : ALIGN(4K) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K) {
        *(.limine_requests)
        *(.data .data.*)
    }

    .bss : ALIGN(4K) {
        *(.bss .bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.note .note.*)
        *(.eh_frame)
    }
}
