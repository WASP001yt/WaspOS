[bits 32]
global start

extern kmain

start:
    mov esp, 0x90000
    call kmain

hang:
    cli
    hlt
    jmp hang
