#include <stdint.h>
#include <stddef.h>
#include <limine.h>

extern "C" {
#include "drivers/video/fb.h"
#include "drivers/video/terminal.h"
#include "drivers/video/gfx.h"
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
#include "shell/shell.h"
#include "lib/printf.h"
}

#include "gui/desktop.hpp"
#include "gui/term_app.hpp"

#include "config.h"

inline void *operator new(size_t, void *ptr) noexcept { return ptr; }

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

static uint64_t read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));
    return cr3;
}

// Storage for TerminalApp — constructed manually after heap_init
static uint8_t term_app_storage[sizeof(TerminalApp)] __attribute__((aligned(16)));
static TerminalApp *term_app = nullptr;

extern "C" void kernel_main(void)
{
    if (!fb_request.response || fb_request.response->framebuffer_count < 1)
        for (;;) __asm__ volatile("hlt");

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];

    fb_init(fb);
    gdt_init();
    tss_set_kernel_stack(gdt_get_exception_stack());
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
    int fd = vfs_open("/etc/motd", O_WRONLY | O_CREAT);
    if (fd >= 0)
    {
        const char *motd = "Welcome to TermuOS\n";
        vfs_write(fd, motd, 19);
        vfs_close(fd);
    }

    scheduler_init();
    pit_init(100);
    mouse_init();

#ifdef CONFIG_NET
    pci_init();
    virtio_net_init();
#endif

    // Construct TerminalApp in-place after all subsystems are ready
    term_app = new (term_app_storage) TerminalApp();

    Desktop::instance().init(fb_colour(10, 10, 20));
    mouse_set_screen(fb->width, fb->height);
    Desktop::instance().add_app(term_app);
    term_app->launch();
    Desktop::instance().run();
}
