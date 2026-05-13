OUTPUT_FORMAT(binary)
ENTRY(_entry)

SECTIONS
{
    . = 0x0;

    .text : {
        /* RTECH Header */
        BYTE(0x52) BYTE(0x54) BYTE(0x45) BYTE(0x43) BYTE(0x48) BYTE(0x30) BYTE(0x31) BYTE(0x00) /* RTECH01\0 */
        QUAD(entry_point)     /* entry_offset */
        QUAD(0)               /* reserved */
        QUAD(0)               /* reserved */

        entry_point = . ;
        *(.text)
    }

    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}
