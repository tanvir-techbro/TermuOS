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
#include "drivers/net/pci.h"
#include "drivers/net/virtio_net.h"
#include "shell/shell.h"
#include "lib/printf.h"
#include "user/elf.h"

extern uint8_t _binary_test_elf_start[];
extern uint8_t _binary_test_elf_end[];

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

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0,%1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ volatile("inb %1,%0" : "=a"(val) : "Nd"(port));
    return val;
}

static void serial_putchar(char c)
{
    while (!(inb(0x3FD) & 0x20));
    outb(0x3F8, (uint8_t)c);
}

static void serial_init(void)
{
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x01);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}

static void dual_putchar(char c)
{
    terminal_putchar(c);
    serial_putchar(c);
}

void kernel_main(void)
{
    if (!fb_request.response || fb_request.response->framebuffer_count < 1)
        for (;;) __asm__ volatile("hlt");

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];

    fb_init(fb);
    serial_init();
    kprintf_set_output(dual_putchar);
    gdt_init();
    tss_set_kernel_stack(gdt_get_exception_stack());
    idt_init(GDT_KERNEL_CODE);
    
    printf("Initializing Memory...\n");
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
    {
        size_t elf_size = (size_t)(_binary_test_elf_end - _binary_test_elf_start);
        int efd = vfs_open("/bin/test", O_WRONLY | O_CREAT);
        if (efd >= 0)
        {
            vfs_write(efd, _binary_test_elf_start, elf_size);
            vfs_close(efd);
        }
    }

    printf("Starting Scheduler...\n");
    scheduler_init();
    pit_init(100);

    pci_init();
    virtio_net_init();

    shell_run();
}
