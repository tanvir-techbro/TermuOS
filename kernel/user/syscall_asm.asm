bits 64

extern syscall_dispatch

; Kernel stack for syscall handling
section .bss
align 16
syscall_kernel_stack_bottom:
    resb 8192
global syscall_kernel_stack_top
syscall_kernel_stack_top:

; Scratch space to save user RSP while switching stacks
syscall_user_rsp: resq 1

section .text

global syscall_entry
syscall_entry:
    ; CPU has set: RCX=user RIP, R11=user RFLAGS, CS/SS=kernel selectors
    ; RSP is still user RSP — save it and switch to kernel stack

    mov [rel syscall_user_rsp], rsp
    lea rsp, [rel syscall_kernel_stack_top]

    ; Save caller-saved regs we use
    push rcx    ; user RIP
    push r11    ; user RFLAGS

    ; syscall_dispatch(num=rax, a=rdi, b=rsi, c=rdx)
    ; rax=syscall number, rdi/rsi/rdx=args (Linux ABI)
    mov  rcx, rdx       ; shift rdx -> rcx for 4th C arg
    mov  rdx, rsi       ; rsi -> rdx
    mov  rsi, rdi       ; rdi -> rsi
    mov  rdi, rax       ; num -> rdi
    ; rdx=b, rcx=c are already set above; rsi=a, rdi=num

    ; actually dispatch(num, a, b, c) = (rdi, rsi, rdx, rcx)
    call syscall_dispatch   ; return value in rax

    pop  r11    ; user RFLAGS
    pop  rcx    ; user RIP

    ; Restore user stack
    mov rsp, [rel syscall_user_rsp]

    o64 sysret      ; 64-bit sysret
