#include "userspace.h"
#include "syscall.h"
#include "elf.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../arch/x86_64/gdt.h"
#include "../lib/printf.h"
#include <stdint.h>

#define USER_STACK_SIZE 16384 // 16KB user stack
#define USER_STACK_TOP 0x7fffffffe000ULL

extern void jump_to_userspace(uint64_t entry, uint64_t user_stack);

void userspace_init(void)
{
    syscall_init();
    kprintf("Userspace: ready.\n");
}

// Load and execute an ELF binary in userspace
int exec(const char *path)
{
    kprintf("exec: loading %s\n", path);

    uint64_t entry = elf_load(path);
    if (!entry)
    {
        kprintf("exec: failed to load %s\n", path);
        return -1;
    }

    // Allocate user stack
    uint64_t cr3;
    __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));

    uint64_t stack_top = USER_STACK_TOP;
    for (uint64_t va = stack_top - USER_STACK_SIZE; va < stack_top; va += PAGE_SIZE)
    {
        void *phys = pmm_alloc();
        if (!phys)
        {
            kprintf("exec: no memory for stack\n");
            return -1;
        }
        uint8_t *p = (uint8_t *)PHYS_TO_VIRT(phys);
        for (int i = 0; i < PAGE_SIZE; i++)
            p[i] = 0;
        vmm_map(cr3, va, (uint64_t)phys, VMM_USER_FLAGS);
    }

    // Set kernel stack in TSS for syscall returns
    extern uint8_t syscall_kernel_stack_top[];
    tss_set_kernel_stack((uint64_t)syscall_kernel_stack_top);

    kprintf("exec: jumping to userspace entry=0x%x stack=0x%x\n",
            entry, stack_top - 8);

    jump_to_userspace(entry, stack_top - 8);
    return 0; // unreachable
}