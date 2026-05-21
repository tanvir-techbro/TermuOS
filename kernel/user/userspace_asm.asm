BITS 64

global enter_userspace

section .text

enter_userspace:

    cli

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; stack segment
    push 0x23

    ; user rsp
    push rsi

    ; rflags
    pushfq

    ; code segment
    push 0x1B

    ; user rip
    push rdi

    iretq
