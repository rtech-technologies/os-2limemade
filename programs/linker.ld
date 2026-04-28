OUTPUT_FORMAT(binary)

SECTIONS
{
    . = 0x0;

    /* RSL Binary Header (32 bytes) */
    .header : {
        BYTE(0x52); BYTE(0x53); BYTE(0x4C); BYTE(0x31); /* "RSL1" Magic */
        QUAD(32);                                       /* Entry Offset */
        QUAD(16384);                                    /* Stack Size (16KB) */
        QUAD(0);                                        /* Reserved */
    }

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
        *(.bss*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}
