MEMORY {
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2M
    ram(rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(init);

SECTIONS {
    . = ORIGIN(flash) + 0x100;

    .text : {
        *(.entry)
        *(.text*)
    }
    .rodata : { *(.rodata*) }

    . = ORIGIN(ram);

    .data : { *(.data*) }
    .bss : {
        __bss_start = .;
        *(.bss*)
        __bss_end = .;
    }

    __ram_start = .;
    __ram_end = ORIGIN(ram) + LENGTH(ram);
}
