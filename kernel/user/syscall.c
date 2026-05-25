#include "syscall.h"
#include "../arch/x86_64/gdt.h"
#include "../drivers/video/terminal.h"
#include "../sched/scheduler.h"
#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

extern volatile uint64_t timer_ticks;
extern void syscall_entry(void);

/* ── MSR helpers ─────────────────────────────────────────────────────────── */

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

#define MSR_EFER   0xC0000080
#define MSR_STAR   0xC0000081
#define MSR_LSTAR  0xC0000082
#define MSR_SFMASK 0xC0000084

static int syscall_supported(void)
{
    uint32_t edx;
    __asm__ volatile(
        "movl $0x80000001, %%eax\n"
        "cpuid\n"
        : "=d"(edx) :: "eax", "ebx", "ecx");
    return (edx >> 11) & 1;
}

/* ── brk ─────────────────────────────────────────────────────────────────── */

/*
 * musl uses brk() to manage its heap. We give userspace a 4 MB heap region
 * starting just above the program break.  brk(0) returns current break;
 * brk(addr) sets it.  We back every new page with physical memory on demand.
 */
#define USER_BRK_BASE  0x0000000000400000ULL   /* just above musl's top ~0x2b0000 */
#define USER_BRK_MAX   0x0000000010000000ULL

static uint64_t current_brk = 0;

pagemap_t current_user_pm;   /* dummy - elf_load removed */

static uint64_t sys_brk(uint64_t addr)
{
    if (addr == 0 || addr < current_brk)
        return current_brk;

    if (addr > USER_BRK_MAX)
        return current_brk;   /* refuse; musl will fall back to mmap */

    /* Map new pages between current_brk and addr */
    uint64_t start = (current_brk + 0xFFF) & ~0xFFFULL;
    uint64_t end   = (addr        + 0xFFF) & ~0xFFFULL;

    for (uint64_t page = start; page < end; page += PAGE_SIZE)
    {
        void *phys = pmm_alloc();
        if (!phys)
            return current_brk;   /* OOM — return old break */
        vmm_map(current_user_pm, page, (uint64_t)phys,
                VMM_PRESENT | VMM_WRITE | VMM_USER);
    }

    current_brk = addr;
    return current_brk;
}

/* ── mmap ────────────────────────────────────────────────────────────────── */

/*
 * musl uses mmap for:
 *   - anonymous memory (malloc fallback, thread stacks)
 *   - mapping files
 *
 * We implement anonymous mmap only (MAP_ANON).  File-backed mmap returns
 * ENOSYS for now; musl falls back to read() in that case.
 *
 * Linux mmap flags we care about:
 */
#define MMAP_PROT_READ   0x1
#define MMAP_PROT_WRITE  0x2
#define MMAP_MAP_PRIVATE 0x2
#define MMAP_MAP_ANON    0x20
#define MMAP_MAP_FIXED   0x10

/* Simple bump allocator for anonymous mmap regions */
#define MMAP_BASE 0x0000000020000000ULL   /* 512 MB */
#define MMAP_MAX  0x0000000080000000ULL   /* 2   GB */
static uint64_t mmap_bump = MMAP_BASE;

static uint64_t sys_mmap(uint64_t addr, uint64_t len, uint64_t prot,
                          uint64_t flags, uint64_t fd, uint64_t off)
{
    (void)prot; (void)off;

    if (!(flags & MMAP_MAP_ANON))
        return (uint64_t)-1;   /* ENOSYS — file mmap not supported */

    if (len == 0)
        return (uint64_t)-1;

    uint64_t pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t virt;

    if ((flags & MMAP_MAP_FIXED) && addr != 0)
        virt = addr;
    else
    {
        virt = (mmap_bump + 0xFFF) & ~0xFFFULL;
        mmap_bump = virt + pages * PAGE_SIZE;
        if (mmap_bump > MMAP_MAX)
            return (uint64_t)-1;
    }

    for (uint64_t i = 0; i < pages; i++)
    {
        void *phys = pmm_alloc();
        if (!phys)
            return (uint64_t)-1;
        vmm_map(current_user_pm, virt + i * PAGE_SIZE, (uint64_t)phys,
                VMM_PRESENT | VMM_WRITE | VMM_USER);
    }

    return virt;
}

static uint64_t sys_munmap(uint64_t addr, uint64_t len)
{
    /* TODO: free pages and reclaim PMM frames */
    (void)addr; (void)len;
    return 0;
}

/* ── fstat ───────────────────────────────────────────────────────────────── */

/*
 * musl calls fstat on fd 1 (stdout) at startup to decide if it's a tty.
 * We return a minimal stat struct with st_mode = S_IFCHR (character device)
 * for fd 0/1/2, and a regular file stat for everything else.
 *
 * Linux stat64 layout (x86-64):
 */
struct kernel_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t  __unused[3];
};

#define S_IFCHR  0020000
#define S_IFREG  0100000

static uint64_t sys_fstat(uint64_t fd, uint64_t buf_addr)
{
    struct kernel_stat *st = (struct kernel_stat *)buf_addr;

    /* zero the whole struct */
    uint8_t *p = (uint8_t *)st;
    for (size_t i = 0; i < sizeof(*st); i++) p[i] = 0;

    if (fd == 0 || fd == 1 || fd == 2)
    {
        st->st_mode    = S_IFCHR | 0620;
        st->st_rdev    = 0x0501;   /* tty major/minor */
        st->st_blksize = 1024;
    }
    else
    {
        uint32_t type;
        uint64_t size;
        /* We don't have an fd→path lookup, so return a generic regular file */
        st->st_mode    = S_IFREG | 0644;
        st->st_blksize = 512;
        st->st_size    = 0;
    }

    return 0;
}

/* ── existing syscalls ───────────────────────────────────────────────────── */

static uint64_t sys_exit(uint64_t code)
{
    kprintf("[kernel] process exited: %u\n", code);
    for (;;) __asm__ volatile("hlt");
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
    return (uint64_t)vfs_read((int)fd, (void *)buf, (size_t)len);
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

static uint64_t sys_getpid(void)
{
    return 1;
}

static uint64_t sys_yield(void)
{
    scheduler_yield();
    return 0;
}

static uint64_t sys_sleep(uint64_t ticks)
{
    uint64_t target = timer_ticks + ticks;
    while (timer_ticks < target)
        __asm__ volatile("hlt");
    return 0;
}

static uint64_t sys_uptime(void)
{
    return timer_ticks;
}

/* ── dispatch ────────────────────────────────────────────────────────────── */

uint64_t syscall_dispatch(uint64_t num, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f)
{
    switch (num)
    {
    case SYS_READ:    return sys_read(a, b, c);
    case SYS_WRITE:   return sys_write(a, b, c);
    case SYS_OPEN:    return sys_open(a, b, c);
    case SYS_CLOSE:   return sys_close(a);
    case SYS_FSTAT:   return sys_fstat(a, b);
    case SYS_MMAP:    return sys_mmap(a, b, c, d, e, f);
    case SYS_MUNMAP:  return sys_munmap(a, b);
    case SYS_BRK:     return sys_brk(a);
    case SYS_GETPID:  return sys_getpid();
    case SYS_YIELD:   return sys_yield();
    case SYS_SLEEP:   return sys_sleep(a);
    case SYS_EXIT:    return sys_exit(a);
    case SYS_UPTIME:  return sys_uptime();
    default:
        kprintf("[kernel] unknown syscall %llu\n", num);
        return (uint64_t)-1;
    }
}

/* ── init ────────────────────────────────────────────────────────────────── */

void syscall_init(void)
{
    kprintf("Syscall: checking CPU support...\n");
    if (!syscall_supported())
    {
        kprintf("Syscall: not supported by CPU.\n");
        return;
    }

    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1;
    efer &= ~(1ULL << 11);
    wrmsr(MSR_EFER, efer);

    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)GDT_KERNEL_CODE << 32);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_SFMASK, (1 << 9) | (1 << 10));

    kprintf("Syscall: ready.\n");
}