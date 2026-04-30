bits 64

global gdt_flush
gdt_flush:
    lgdt [rdi]

    push rsi
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    mov ax, dx
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global tss_flush
tss_flush:
    mov ax, di
    ltr ax
    ret
