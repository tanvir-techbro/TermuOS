section .bss
align 16
stack_bottom:
    resb 16384   ; 16 KiB stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov rsp, stack_top   ; set up a valid stack
    mov rbp, 0           ; clear base pointer (marks end of stack frames)
    call kernel_main
.hang:
    hlt
    jmp .hang