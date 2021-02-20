MEMORY {
    flash(rwx) : ORIGIN = 0x10000000, LENGTH = 2M;
    ram(rwx) : ORIGIN = 0x20000000, LENGTH = 256K;
}

SECTIONS {
    . = ORIGIN(ram);

    .text : { *(.text) }

    .data : { *(.data) }

    .rodata : { *(.rodata) }

    .bss : {
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    }

    ASSERT(ABSOLUTE(.) <= ORIGIN(ram) + LENGTH(ram) - 256);

    . = ORIGIN(ram) + LENGTH(ram) - 256;

    .init : {
        KEEP(*(.init));
        ASSERT(ABSOLUTE(.) <= ORIGIN(ram) + LENGTH(ram));
    }
}
