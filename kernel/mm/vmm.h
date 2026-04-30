#pragma once
#include <stdint.h>
#include <stddef.h>

// page flags
#define VMM_PRESENT (1ULL << 0)
#define VMM_WRITE (1ULL << 1)
#define VMM_USER (1ULL << 2)
#define VMM_NX (1ULL << 63) // no-execute (requires EFER.NXE)

#define VMM_KERNEL_FLAGS (VMM_PRESENT | VMM_WRITE)
#define VMM_USER_FLAGS (VMM_PRESENT | VMM_WRITE | VMM_USER)

extern uint64_t hhdm_base;

#define PHYS_TO_VIRT(p) ((void*)((uint64_t)(p) + hhdm_base))
#define VIRT_TO_PHYS(v) ((uint64_t)(v) - hhdm_base)

typedef uint64_t pagemap_t;

void vmm_init(uint64_t hhdm, pagemap_t kernel_pml4);
pagemap_t vmm_new_pagemap(void);
void vmm_map(pagemap_t pm, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(pagemap_t pm, uint64_t virt);
void vmm_switch(pagemap_t pm);
uint64_t vmm_virt_to_phys(pagemap_t pm, uint64_t virt);