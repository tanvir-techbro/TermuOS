#include "userspace.h"
#include "syscall.h"
#include "../arch/x86_64/gdt.h"
#include "../lib/printf.h"
#include <stdint.h>

void userspace_init(void)
{
    syscall_init();
}

void jump_userspace(uint64_t entry, uint64_t stack)
{
    tss_set_kernel_stack(gdt_get_exception_stack());
    kprintf("userspace: jumping to 0x%x\n", entry);
    enter_userspace(entry, stack);
}