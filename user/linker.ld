ENTRY(_start)

SECTIONS {
    . = 0x400000000;   /* 16GB — boot.s identity map(0~1GB) 밖 */

    .text   : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }
    .bss    : { *(.bss .bss.*) *(COMMON) }

    /DISCARD/ : { *(.eh_frame) *(.note*) *(.comment) *(.gnu*) }
}
