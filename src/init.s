.section entry, "ex"

    @ Prepare address of XIP_SSI_BASE.
    mov r0, #0x18
    lsl r0, #24

configure_CTRLR0:
    mov r1, #0

    @ Set frame format to Quad-SPI.
    or r1, #2

    @ Set frame size to 32 bit.
    mov r2, #32
    lsl r2, #16
    or r1, r1, r2

    str r1, [r0, 0x00]
