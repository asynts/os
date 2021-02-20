.section entry, "ex"

    @ Prepare address of XIP_SSI_BASE.
    mov r0, #0x18
    lsl r0, #24

    @ Prepare value.
    mov r1, #0

    @ Write value to XIP_SSI_BASE.CTRLR0.
    str r1, [r0, #0x00]
