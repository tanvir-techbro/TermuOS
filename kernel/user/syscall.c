#include "syscall.h"
#include "../arch/x86_64/gdt.h"
#include "../drivers/video/terminal.h"
#include "../fs/vfs.h"
#include "../lib/printf.h"
#include <stdint.h>

// ─── Syscall entry (defined in syscall.asm) ───────────────────────────────────
extern void syscall_entry(void);

// ─── MSR helpers ─────────────────────────────────────────────────────────────

static void wrmsr(uint32_t msr, uint64_t val)
{
    __asm__ volatile("wrmsr" ::"c"(msr),
                     "a"((uint32_t)(val & 0xffffffff)),
                     "d"((uint32_t)(val >> 32)));
}

#define MSR_EFER 0xC0000080   // Extended Feature Enable Register
#define MSR_STAR 0xC0000081   // Syscall target CS/SS + user CS/SS
#define MSR_LSTAR 0xC0000082  // Syscall entry RIP (long mode)
#define MSR_SFMASK 0xC0000084 // Flags to clear on syscall

// ─── Syscall implementations ─────────────────────────────────────────────────

static uint64_t sys_exit(uint64_t code)
{
    kprintf("[kernel] process exited with code %u\n", code);
    // For now just halt — later this will kill the process and schedule next
    for (;;)
        __asm__ volatile("hlt");
    return 0;
}

static uint64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t len)
{
    // fd 1 = stdout, fd 2 = stderr — write to terminal
    if (fd == 1 || fd == 2)
    {
        const char *buf = (const char *)buf_addr;
        for (uint64_t i = 0; i < len; i++)
            terminal_putchar(buf[i]);
        return len;
    }
    return (uint64_t)vfs_write((int)fd, (const void *)buf_addr, (size_t)len);
}

static uint64_t sys_read(uint64_t fd, uint64_t buf_addr, uint64_t len)
{
    (void)fd;
    (void)buf_addr;
    (void)len;
    return 0; // stub
}

static uint64_t sys_open(uint64_t path_addr, uint64_t flags, uint64_t mode)
{
    (void)mode;
    return (uint64_t)vfs_open((const char *)path_addr, (uint32_t)flags);
}

static uint64_t sys_close(uint64_t fd)
{
    return (uint64_t)vfs_close((int)fd);
}

// ─── Dispatch (called from syscall.asm) ──────────────────────────────────────

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

// ─── Init ─────────────────────────────────────────────────────────────────────

void syscall_init(void)
{
    // Enable syscall/sysret in EFER
    uint64_t efer;
    __asm__ volatile("rdmsr" : "=A"(efer) : "c"(MSR_EFER));
    efer |= 1; // SCE bit
    wrmsr(MSR_EFER, efer);

    // STAR: bits[47:32] = kernel CS (syscall sets CS to this, SS to this+8)
    //       bits[63:48] = user CS-16 (sysret sets CS to this+16, SS to this+8)
    // kernel CS = 0x08, user CS = 0x18, user DS = 0x20
    // STAR[47:32] = 0x0008 (kernel CS)
    // STAR[63:48] = 0x0013 (user CS - 16 = 0x18 - 8 = 0x10, but sysret adds 16)
    // Actually: sysret sets CS = STAR[63:48]+16, SS = STAR[63:48]+8
    // We want CS=0x1B (0x18|3), SS=0x23 (0x20|3) for ring 3
    // So STAR[63:48] = 0x18 - 16 = 0x08? No...
    // Correct: STAR[63:48] = user_cs - 16 where user_cs is the ring3 CS
    // user_cs = 0x18, so STAR[63:48] = 0x18 - 16? That's wrong.
    // Intel manual: sysret64 loads CS from STAR[63:48]+16, SS from STAR[63:48]+8
    // So: STAR[63:48] = 0x18 - 16 = 0x08 -- but that collides with kernel CS
    // Correct formula: STAR[63:48] should be (user_code_sel - 16)
    // user_code_sel = 0x18, so STAR[63:48] = 0x18-16 = 0x08...
    // Actually the layout needs: user_data at 0x18+8 = 0x20 for sysret to work
    // With our GDT: kernel_code=0x08, kernel_data=0x10, user_code=0x18, user_data=0x20
    // sysret sets CS = STAR[63:48]+16, SS = STAR[63:48]+8
    // For CS=0x1B(0x18|3): STAR[63:48]+16 = 0x18 => STAR[63:48] = 0x08
    // For SS=0x23(0x20|3): STAR[63:48]+8  = 0x20 => STAR[63:48] = 0x18 -- CONFLICT
    // The trick: user_data must be at user_code+8. Our layout has that (0x18,0x20). ✓
    // STAR[63:48] = 0x10 => CS = 0x10+16 = 0x20 (WRONG)
    // STAR[63:48] must satisfy: +16=user_code_base AND +8=user_data_base
    // user_code=0x18, user_data=0x20=0x18+8 ✓
    // STAR[63:48] = 0x18-16 = 0x08 => CS=0x08+16=0x18 ✓, SS=0x08+8=0x10 (kernel data, WRONG)
    // The ONLY way to make sysret work is: user_code immediately after user_data OR
    // use a different GDT layout. Standard layout: null,kcode,kdata,ucode,udata
    // null=0x00, kcode=0x08, kdata=0x10, ucode=0x18, udata=0x20
    // sysret: CS=STAR[63:48]+16|3, SS=STAR[63:48]+8|3
    // need CS=0x18, SS=0x20 => STAR[63:48]+16=0x18 => base=0x08
    //                          STAR[63:48]+8=0x08+8=0x10 (kdata, ring0!) WRONG
    // Standard fix: swap user_code and user_data order, or add null between kdata and ucode
    // LINUX uses: null=0,kcode=0x10,kdata=0x18,udata=0x20,ucode=0x28
    // Then STAR[63:48]=0x20: CS=0x20+16=0x30? No...
    // Just use: STAR[63:48] = user_data_sel - 8 with RPL=3 set by CPU
    // With our layout swapped: kcode=0x08,kdata=0x10,udata=0x18,ucode=0x20
    // STAR[63:48]=0x10+3=0x13? No, STAR stores plain selector.
    // Simplest fix: reorder GDT so udata comes before ucode

    // For now use the correct STAR value for Linux-style layout we'll adopt:
    // STAR[47:32] = kernel CS = 0x08
    // STAR[63:48] = 0x10 (sysret: CS=0x10+16=0x20=user_code, SS=0x10+8=0x18=user_data)
    // This requires: user_data=0x18, user_code=0x20 — opposite of our current layout!
    // We need to fix the GDT layout. Let's do that now.

    // Updated selectors (see gdt.h):
    // GDT_KERNEL_CODE=0x08, GDT_KERNEL_DATA=0x10
    // GDT_USER_DATA=0x18,   GDT_USER_CODE=0x20   <-- swapped!
    // STAR[63:48]=0x10: sysret CS=0x10+16=0x20=ucode ✓, SS=0x10+8=0x18=udata ✓

    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)GDT_KERNEL_CODE << 32);
    wrmsr(MSR_STAR, star);

    // LSTAR: syscall entry point
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    // SFMASK: clear IF (disable interrupts on syscall entry) + DF
    wrmsr(MSR_SFMASK, (1 << 9) | (1 << 10));

    kprintf("Syscall: MSRs configured.\n");
}