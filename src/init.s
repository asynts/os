.section .init

init:
    @ Prepare mask for GPIO 25.
    mov r0, #1
    lsl r0, #25

    @ Prepare the SIO_BASE address.
    mov r1, #0xd0
    lsl r1, #24

    @ Make GPIO 25 an output.
    str r0, [r1, #0x028]

    @ Turn GPIO 25 on.
    str r0, [r1, #0x014]

    @ Halt.
loop:
    wfi
    b loop
