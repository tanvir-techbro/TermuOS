BITS 64

global enter_userspace

section .text

enter_userspace:
    cli

    mov ax, 0x1b        ; user data (0x18 | RPL3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x1b           ; SS  = user data RPL3
    push rsi            ; user RSP
    push 0x202          ; RFLAGS (IF set)
    push 0x23           ; CS  = user code (0x20 | RPL3)
    push rdi            ; user RIP

    iretq