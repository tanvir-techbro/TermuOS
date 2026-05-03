bits 64

; void gdt_flush(uint64_t gdtr_addr, uint16_t cs, uint16_t ds)
;   rdi = address of GDTR struct
;   rsi = kernel CS selector
;   rdx = kernel DS selector
global gdt_flush
gdt_flush:
    lgdt [rdi]

    ; Reload CS via a far return
    ; Push new CS and return address, then retfq
    push rsi                    ; new CS
    lea  rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ; Reload all data segment registers
    mov ax, dx
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

; void tss_flush(uint16_t tss_sel)
;   rdi = TSS selector
global tss_flush
tss_flush:
    mov ax, di
    ltr ax
    ret
