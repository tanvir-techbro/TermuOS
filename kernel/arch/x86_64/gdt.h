#pragma once
#include <stdint.h>

// GDT segment selectors
// Order matters for sysret: udata must come before user code
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_DATA    0x18  // must come before user code for sysret
#define GDT_USER_CODE    0x20  // user code selector
#define GDT_TSS_LOW      0x28

// User selectors with RPL=3 set
#define GDT_USER_CODE_RPL3  (GDT_USER_CODE | 3)
#define GDT_USER_DATA_RPL3  (GDT_USER_DATA | 3)

void gdt_init(void);
void tss_set_kernel_stack(uint64_t rsp);
uint64_t gdt_get_exception_stack(void);