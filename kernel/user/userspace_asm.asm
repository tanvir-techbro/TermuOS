bits 64

; void jump_to_userspace(uint64_t entry, uint64_t user_stack)
;   rdi = entry point
;   rsi = user stack pointer
;
; Use iretq to switch to ring 3:
;   push SS
;   push RSP
;   push RFLAGS
;   push CS
;   push RIP
;   iretq

global jump_to_userspace
jump_to_userspace:
    ; rdi = entry, rsi = user_stack

    ; Build iretq frame on current (kernel) stack
    ; SS = user data selector | 3
    push 0x1b           ; user SS  = GDT_USER_DATA | 3 = 0x18|3 = 0x1b
    push rsi            ; user RSP
    pushfq              ; RFLAGS
    pop  rax
    or   rax, (1 << 9) ; set IF (enable interrupts in userspace)
    push rax
    push 0x23           ; user CS  = GDT_USER_CODE | 3 = 0x20|3 = 0x23
    push rdi            ; user RIP = entry

    iretq
