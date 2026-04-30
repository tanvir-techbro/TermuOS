#pragma once
#include <stdint.h>

typedef struct
{
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

// IRQ handler function type
typedef void (*irq_handler_t)(registers_t *r);

void idt_init(uint16_t cs);
void idt_register_irq(uint8_t irq, irq_handler_t handler);