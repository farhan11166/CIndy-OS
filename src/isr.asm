global isr33

isr33:
    cli
    pusha

    call keyboard_handler

    popa
    sti
    iret