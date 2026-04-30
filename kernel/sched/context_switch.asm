bits 64

; void context_switch(uint64_t *old_rsp, uint64_t new_rsp)
;   rdi = pointer to old thread's rsp field (we save RSP here)
;   rsi = new thread's saved RSP value

global context_switch
context_switch:
    ; Save callee-saved registers of the current (old) thread
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save current RSP into old thread's rsp field
    mov [rdi], rsp

    ; Switch to new thread's stack
    mov rsp, rsi

    ; Restore new thread's callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Return into the new thread with interrupts enabled
    ; (either its entry point or wherever it last yielded)
    sti
    ret
