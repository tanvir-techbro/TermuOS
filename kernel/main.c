#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include "drivers/video/fb.h"
#include "drivers/video/terminal.h"
#include "drivers/input/keyboard.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/pit.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "sched/scheduler.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "user/userspace.h"
#include "user/init_bin.h"
#include "shell/shell.h"
#include "lib/printf.h"

LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_start"))) static volatile LIMINE_REQUESTS_START_MARKER

    __attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request fb_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST,
        .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0};

__attribute__((used, section(".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER

    static uint64_t
    read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void kernel_main(void)
{
    if (!fb_request.response || fb_request.response->framebuffer_count < 1)
        for (;;)
            __asm__ volatile("hlt");

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];

    fb_init(fb);
    terminal_init();
    terminal_set_size(fb->width, fb->height);
    terminal_set_bg(0x0d, 0x0d, 0x0d);
    terminal_set_fg(0x00, 0xff, 0x88);

    kprintf("TermuOS booting...\n");

    gdt_init();
    idt_init(GDT_KERNEL_CODE);
    pmm_init(memmap_request.response);
    vmm_init(hhdm_request.response->offset, read_cr3());
    heap_init();
    keyboard_init();

    vfs_init();
    vfs_node_t *root = ramfs_create_root();
    vfs_mount("/", root);
    vfs_mkdir("/etc");
    vfs_mkdir("/home");
    vfs_mkdir("/home/root");
    vfs_mkdir("/bin");

    // Write motd
    int fd = vfs_open("/etc/motd", O_WRONLY | O_CREAT);
    if (fd >= 0)
    {
        const char *motd = "Welcome to TermuOS!\n";
        vfs_write(fd, motd, 20);
        vfs_close(fd);
    }

    // Embed init binary into /bin/init
    fd = vfs_open("/bin/init", O_WRONLY | O_CREAT);
    if (fd >= 0)
    {
        vfs_write(fd, init_elf, init_elf_len);
        vfs_close(fd);
        kprintf("Loaded /bin/init (%u bytes)\n", init_elf_len);
    }

    scheduler_init();
    pit_init(100);
    userspace_init();

    // Shell runs directly — keyboard polling in main thread
    // Scheduler handles any background threads created by commands
    __asm__ volatile("sti");
    shell_run();
}