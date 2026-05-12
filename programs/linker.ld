OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
    . = 0x0;
    .header : {
        *(.header)
    }
    .text : {
        *(.text)
        *(.text.*)
    }
    .data : {
        *(.data)
        *(.data.*)
    }
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    .bss : {
        *(.bss)
        *(COMMON)
    }
}
