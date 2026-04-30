; Tiny userspace init program
; Calls sys_write(1, msg, len) then sys_exit(0)
bits 64

section .text
global _start
_start:
    ; write(1, msg, 13)
    mov rax, 1          ; SYS_WRITE
    mov rdi, 1          ; fd = stdout
    lea rsi, [rel msg]
    mov rdx, 13         ; length
    syscall

    ; exit(0)
    mov rax, 0          ; SYS_EXIT
    mov rdi, 0          ; code = 0
    syscall

section .data
msg: db "Hello from ring 3!", 10, 0
