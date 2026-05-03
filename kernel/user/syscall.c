#include "syscall.h"
#include "../arch/x86_64/gdt.h"
#include "../drivers/video/terminal.h"
#include "../fs/vfs.h"
#include "../lib/printf.h"
#include <stdint.h>

extern void syscall_entry(void);

static void wrmsr(uint32_t msr, uint64_t val)
{
    __asm__ volatile("wrmsr" ::"c"(msr),
                     "a"((uint32_t)(val & 0xffffffff)),
                     "d"((uint32_t)(val >> 32)));
}

static uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

static int syscall_supported(void)
{
    // Check CPUID extended features for syscall/sysret support
    uint32_t edx;
    __asm__ volatile(
        "movl $0x80000001, %%eax\n"
        "cpuid\n"
        : "=d"(edx)::"eax", "ebx", "ecx");
    return (edx >> 11) & 1; // bit 11 = SYSCALL/SYSRET
}

static uint64_t sys_exit(uint64_t code)
{
    kprintf("[kernel] process exited: %u\n", code);
    for (;;)
        __asm__ volatile("hlt");
    return 0;
}

static uint64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t len)
{
    if (fd == 1 || fd == 2)
    {
        const char *buf = (const char *)buf_addr;
        for (uint64_t i = 0; i < len; i++)
            terminal_putchar(buf[i]);
        return len;
    }
    return (uint64_t)vfs_write((int)fd, (const void *)buf_addr, (size_t)len);
}

static uint64_t sys_read(uint64_t fd, uint64_t buf, uint64_t len)
{
    (void)fd;
    (void)buf;
    (void)len;
    return 0;
}

static uint64_t sys_open(uint64_t path, uint64_t flags, uint64_t mode)
{
    (void)mode;
    return (uint64_t)vfs_open((const char *)path, (uint32_t)flags);
}

static uint64_t sys_close(uint64_t fd)
{
    return (uint64_t)vfs_close((int)fd);
}

uint64_t syscall_dispatch(uint64_t num, uint64_t a, uint64_t b, uint64_t c)
{
    switch (num)
    {
    case SYS_EXIT:
        return sys_exit(a);
    case SYS_WRITE:
        return sys_write(a, b, c);
    case SYS_READ:
        return sys_read(a, b, c);
    case SYS_OPEN:
        return sys_open(a, b, c);
    case SYS_CLOSE:
        return sys_close(a);
    default:
        kprintf("[kernel] unknown syscall %u\n", num);
        return (uint64_t)-1;
    }
}

void syscall_init(void)
{
    kprintf("Syscall: checking CPU support...\n");
    if (!syscall_supported())
    {
        kprintf("Syscall: not supported by CPU, exec disabled.\n");
        return;
    }
    kprintf("Syscall: supported.\n");

    kprintf("Syscall: writing EFER...\n");
    uint64_t efer = rdmsr(MSR_EFER);
    kprintf("Syscall: EFER=0x%x\n", efer);
    efer |= 1;
    efer &= ~(1ULL << 11);
    wrmsr(MSR_EFER, efer);

    kprintf("Syscall: writing STAR...\n");
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)GDT_KERNEL_CODE << 32);
    wrmsr(MSR_STAR, star);

    kprintf("Syscall: writing LSTAR...\n");
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    kprintf("Syscall: writing SFMASK...\n");
    wrmsr(MSR_SFMASK, (1 << 9) | (1 << 10));

    kprintf("Syscall: ready.\n");
}