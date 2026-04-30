#include "gdt.h"
#include "../../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

// ─── GDT entry formats ────────────────────────────────────────────────────────

typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;      // present, DPL, S, type
    uint8_t flags_limit; // granularity, size, long mode, limit high
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

// TSS descriptor is 16 bytes (two GDT slots)
typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t flags_limit;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) gdt_tss_entry_t;

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

// ─── TSS ─────────────────────────────────────────────────────────────────────

typedef struct
{
    uint32_t reserved0;
    uint64_t rsp[3]; // RSP0, RSP1, RSP2 (ring 0-2 stacks)
    uint64_t reserved1;
    uint64_t ist[7]; // interrupt stack table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

// ─── GDT layout ──────────────────────────────────────────────────────────────
//
// Index  Offset  Description
//   0    0x00    Null descriptor
//   1    0x08    Kernel code (ring 0, 64-bit)
//   2    0x10    Kernel data (ring 0)
//   3    0x18    User code   (ring 3, 64-bit)
//   4    0x20    User data   (ring 3)
//   5    0x28    TSS low     (16-byte descriptor)
//   6    0x30    TSS high

typedef struct
{
    gdt_entry_t null;
    gdt_entry_t kernel_code;
    gdt_entry_t kernel_data;
    gdt_entry_t user_data; // 0x18 — must be before user_code for sysret
    gdt_entry_t user_code; // 0x20
    gdt_tss_entry_t tss;
} __attribute__((packed)) gdt_t;

static gdt_t gdt;
static gdtr_t gdtr;
static tss_t tss;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void set_entry(gdt_entry_t *e, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t flags)
{
    e->base_low = base & 0xffff;
    e->base_mid = (base >> 16) & 0xff;
    e->base_high = (base >> 24) & 0xff;
    e->limit_low = limit & 0xffff;
    e->flags_limit = ((limit >> 16) & 0x0f) | (flags << 4);
    e->access = access;
}

static void set_tss_entry(gdt_tss_entry_t *e, uint64_t base, uint32_t limit)
{
    e->limit_low = limit & 0xffff;
    e->base_low = base & 0xffff;
    e->base_mid = (base >> 16) & 0xff;
    e->access = 0x89; // present, ring 0, 64-bit TSS available
    e->flags_limit = (limit >> 16) & 0x0f;
    e->base_high = (base >> 24) & 0xff;
    e->base_upper = (base >> 32) & 0xffffffff;
    e->reserved = 0;
}

// Defined in gdt.asm — loads GDT and reloads segment registers
extern void gdt_flush(uint64_t gdtr_addr, uint16_t cs, uint16_t ds);
extern void tss_flush(uint16_t tss_sel);

// ─── Init ─────────────────────────────────────────────────────────────────────

void gdt_init(void)
{
    // Null descriptor
    set_entry(&gdt.null, 0, 0, 0, 0);

    // Kernel code: present, DPL=0, long mode
    set_entry(&gdt.kernel_code, 0, 0xfffff, 0x9a, 0x2);
    // Kernel data: present, DPL=0
    set_entry(&gdt.kernel_data, 0, 0xfffff, 0x92, 0x0);
    // User data: present, DPL=3 (must be at 0x18 for sysret)
    set_entry(&gdt.user_data, 0, 0xfffff, 0xf2, 0x0);
    // User code: present, DPL=3, long mode (must be at 0x20 for sysret)
    set_entry(&gdt.user_code, 0, 0xfffff, 0xfa, 0x2);

    // TSS
    for (size_t i = 0; i < sizeof(tss); i++)
        ((uint8_t *)&tss)[i] = 0;
    tss.iopb_offset = sizeof(tss);

    set_tss_entry(&gdt.tss, (uint64_t)&tss, sizeof(tss) - 1);

    // Load GDTR
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    gdt_flush((uint64_t)&gdtr, GDT_KERNEL_CODE, GDT_KERNEL_DATA);
    tss_flush(GDT_TSS_LOW);

    kprintf("GDT: loaded (%u entries, TSS at 0x%x)\n",
            sizeof(gdt) / sizeof(gdt_entry_t), (uint64_t)&tss);
}

void tss_set_kernel_stack(uint64_t rsp)
{
    tss.rsp[0] = rsp;
}