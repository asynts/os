MEMORY {
    ram(rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(init);

SECTIONS {
    . = ORIGIN(ram);

    .text : { *(.text*) }

    .data : { *(.data*) }

    .rodata : { *(.rodata*) }

    .bss : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    }

    .init : {
        . = ORIGIN(ram) + LENGTH(ram) - 256;
        *(.init)

        . = ORIGIN(ram) + LENGTH(ram) - 4;
        init_crc32 = .;
        LONG(0);
    }
}
