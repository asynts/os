.section .init

memcpy:
    ldr r4, 1
    b r4
1
    .word

memset:
    ldr r4, 1
    b r4
1
    .word

init:
    ; Start loading kernel image into ram via DMA.

    ; Hook up functions in bootrom.

    ; Start clearing .bss section via DMA.

    ; Wait for DMA operations to complete.

    ; Hand over control to the kernel.
    b main
