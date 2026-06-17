/* /boot/linker.ld */
ENTRY(_start)

PHDRS
{
    text    PT_LOAD FLAGS((1 << 0) | (1 << 2)); /* Execute + Read */
    rodata  PT_LOAD FLAGS((1 << 2));            /* Read Only */
    data    PT_LOAD FLAGS((1 << 1) | (1 << 2)); /* Write + Read */
}

SECTIONS
{
    . = 0xffffffff80000000;

    .text : ALIGN(0x1000) {
        __text_start = .;
        *(.text .text.*)
        *(.scheduler)
        __text_end = .;
    } :text

    .rodata : ALIGN(0x1000) {
        *(.rodata .rodata.*)
        *(.note.gnu.build-id)
    } :rodata

    .data : ALIGN(0x1000) {
        . = ALIGN(8);
        *(.limine_requests)
        *(.data .data.*)
        . = ALIGN(8);
        __usbh_class_info_start__ = .;
        *(.usbh_class_info)
        __usbh_class_info_end__ = .;
    } :data

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.note.GNU-stack)
    }
}
