bits 64

extern exception_handler

; ─── Macro to push all GPRs ──────────────────────────────────────────────────
%macro PUSH_ALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; ─── Exception common (halts — never truly returns) ──────────────────────────
exception_common:
    PUSH_ALL
    mov rdi, rsp
    call exception_handler
    ; exception_handler halts for exceptions, but for IRQs it returns
    ; This path is for exceptions only (vectors 0-31)
    POP_ALL
    add rsp, 16     ; pop vector + error code
    iretq

; ─── IRQ stubs (vectors 0-31, CPU exceptions) ────────────────────────────────
%macro ISR_NOERR 1
isr_%1:
    push qword 0
    push qword %1
    jmp exception_common
%endmacro

%macro ISR_ERR 1
isr_%1:
    push qword %1
    jmp exception_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

; ─── IRQ common (vectors 32-47, hardware IRQs — MUST iretq cleanly) ──────────
irq_common:
    PUSH_ALL
    mov rdi, rsp
    call exception_handler  ; handles IRQ dispatch + pic_eoi, then returns
    POP_ALL
    add rsp, 16             ; pop vector + dummy error
    iretq

%macro IRQ_STUB 1
isr_%1:
    push qword 0
    push qword %1
    jmp irq_common
%endmacro

IRQ_STUB 32
IRQ_STUB 33
IRQ_STUB 34
IRQ_STUB 35
IRQ_STUB 36
IRQ_STUB 37
IRQ_STUB 38
IRQ_STUB 39
IRQ_STUB 40
IRQ_STUB 41
IRQ_STUB 42
IRQ_STUB 43
IRQ_STUB 44
IRQ_STUB 45
IRQ_STUB 46
IRQ_STUB 47

; ─── Stub pointer tables ─────────────────────────────────────────────────────
section .rodata

global isr_stubs
isr_stubs:
%assign i 0
%rep 32
    dq isr_%+i
%assign i i+1
%endrep

global irq_stubs
irq_stubs:
%assign i 32
%rep 16
    dq isr_%+i
%assign i i+1
%endrep
