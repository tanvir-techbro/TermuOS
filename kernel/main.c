#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#include "drivers/video/fb.h"
#include "drivers/video/terminal.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/pit.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "sched/scheduler.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "drivers/net/pci.h"
#include "drivers/net/virtio_net.h"
#include "gfx/gfx.h"
#include "desktop/desktop.h"
#include "shell/shell.h"
#include "lib/printf.h"

LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_start"))) static volatile LIMINE_REQUESTS_START_MARKER

    __attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request fb_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0};

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
    gfx_init();

    // Init subsystems silently
    gdt_init();
    tss_set_kernel_stack(gdt_get_exception_stack());
    idt_init(GDT_KERNEL_CODE);
    pmm_init(memmap_request.response);
    vmm_init(hhdm_request.response->offset, read_cr3());
    heap_init();
    keyboard_init();
    mouse_init();

    vfs_init();
    vfs_node_t *root = ramfs_create_root();
    vfs_mount("/", root);
    vfs_mkdir("/etc");
    vfs_mkdir("/home");
    vfs_mkdir("/home/root");
    vfs_mkdir("/bin");
    int fd = vfs_open("/etc/motd", O_WRONLY | O_CREAT);
    if (fd >= 0)
    {
        const char *motd = "snoop dog bob steve gary albert bryan dave\n";
        vfs_write(fd, motd, 44);
        vfs_close(fd);
    }

    scheduler_init();
    pit_init(100);

    pci_init();
    virtio_net_init();

    // Launch desktop
    __asm__ volatile("sti");
    desktop_run();
    // After desktop exits — fall back to terminal shell
    terminal_init();
    terminal_set_size(fb->width, fb->height);
    terminal_set_bg(0x0d, 0x0d, 0x0d);
    terminal_set_fg(0x00, 0xff, 0x88);
    kprintf("Returned to shell.\n");
    shell_run();
}