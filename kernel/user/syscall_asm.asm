bits 64

extern syscall_dispatch

section .bss
align 16
syscall_kernel_stack_bottom:
    resb 8192
global syscall_kernel_stack_top
syscall_kernel_stack_top:

syscall_user_rsp: resq 1

section .text

global syscall_entry
syscall_entry:
    ; On entry (Linux x86-64 syscall ABI):
    ;   rax = syscall number
    ;   rdi = arg1, rsi = arg2, rdx = arg3
    ;   r10 = arg4, r8  = arg5, r9  = arg6
    ;   rcx = user RIP (clobbered by syscall instruction)
    ;   r11 = user RFLAGS (clobbered by syscall instruction)

    mov [rel syscall_user_rsp], rsp
    lea rsp, [rel syscall_kernel_stack_top]

    push rcx            ; save user RIP
    push r11            ; save user RFLAGS

    ; Call syscall_dispatch(num, a, b, c, d, e, f)
    ; C ABI:            rdi  rsi rdx rcx r8  r9  [rsp+8]
    ; Linux syscall:    rax  rdi rsi rdx r10 r8  r9
    push r9             ; arg6 -> stack (7th C arg)
    mov  r9,  r8        ; arg5 -> r9
    mov  r8,  r10       ; arg4 -> r8
    mov  rcx, rdx       ; arg3 -> rcx
    mov  rdx, rsi       ; arg2 -> rdx
    mov  rsi, rdi       ; arg1 -> rsi
    mov  rdi, rax       ; num  -> rdi

    call syscall_dispatch

    add  rsp, 8         ; clean up arg6 from stack

    pop  r11            ; restore user RFLAGS
    pop  rcx            ; restore user RIP
    mov  rsp, [rel syscall_user_rsp]
    o64 sysret
