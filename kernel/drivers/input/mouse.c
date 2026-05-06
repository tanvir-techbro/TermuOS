#include "mouse.h"
#include "../../arch/x86_64/idt.h"
#include "../../lib/printf.h"
#include <stdint.h>

#define MOUSE_PORT_DATA 0x60
#define MOUSE_PORT_CMD 0x64
#define MOUSE_IRQ 12

static int mouse_cycle = 0;
static uint8_t mouse_packet[3];
static volatile mouse_state_t state;
static volatile int changed = 0;
static int screen_w = 1280;
static int screen_h = 600;
static volatile int mouse_ready = 0;

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p)
{
    uint8_t v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p));
    return v;
}

static void mouse_wait_write(void)
{
    int timeout = 100000;
    while (timeout-- && (inb(0x64) & 0x02))
        ;
}

static void mouse_wait_read(void)
{
    int timeout = 100000;
    while (timeout-- && !(inb(0x64) & 0x01))
        ;
}

static void mouse_write(uint8_t val)
{
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, val);
}

static uint8_t mouse_read(void)
{
    mouse_wait_read();
    return inb(0x60);
}

// Call from IRQ12 handler
static void mouse_irq(registers_t *r)
{
    (void)r;
    
    if (!mouse_ready)
    {
        inb(0x60);
        return;
    }

    if (!(inb(0x64) & 1))
        return; // no data available

    uint8_t data = inb(MOUSE_PORT_DATA);

    // First byte: flags
    if (mouse_cycle == 0 && !(data & 0x08))
        return;

    mouse_packet[mouse_cycle++] = data;
    if (mouse_cycle < 3)
        return;

    mouse_cycle = 0;

    // Decode packet
    uint8_t flags = mouse_packet[0];
    int dx = (int)mouse_packet[1] - ((flags & 0x10) ? 256 : 0);
    int dy = (int)mouse_packet[2] - ((flags & 0x20) ? 256 : 0);
    dy = -dy; // Y is inverted

    state.dx = dx;
    state.dy = dy;
    state.x += dx;
    state.y += dy;

    if (state.x < 0)
        state.x = 0;
    if (state.x >= screen_w)
        state.x = screen_w - 1;
    if (state.y < 0)
        state.y = 0;
    if (state.y >= screen_h)
        state.y = screen_h - 1;

    state.left = (flags & 0x01) ? 1 : 0;
    state.right = (flags & 0x02) ? 1 : 0;
    state.middle = (flags & 0x04) ? 1 : 0;
    changed = 1;
}

void mouse_init(void)
{
    mouse_ready = 0;
    state.x = screen_w / 2;
    state.y = screen_h / 2;

    // flush any pending data
    while (inb(0x64) & 0x01)
        inb(0x60);

    // disable both devices during init
    outb(0x64, 0xAD); // disable keyboard
    for (volatile int i = 0; i < 10000; i++)
        ;
    outb(0x64, 0xA7); // disable mouse
    for (volatile int i = 0; i < 10000; i++)
        ;

    // flush again
    while (inb(0x64) & 0x01)
        inb(0x60);

    // read and modify controller config
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t config = inb(0x60);
    config |= 0x01;  // enable keyboard interrupt
    config |= 0x02;  // enable mouse interrupt
    config &= ~0x20; // clear mouse disable bit
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, config);

    // re-enable keyboard
    outb(0x64, 0xAE);
    for (volatile int i = 0; i < 10000; i++)
        ;

    // re-enable mouse
    outb(0x64, 0xA8);
    for (volatile int i = 0; i < 10000; i++)
        ;

    // reset mouse
    mouse_write(0xFF);
    mouse_wait_read();
    uint8_t ack = inb(0x60); // should be 0xFA
    if (ack != 0xFA) {
        kprintf("Mouse: reset ack failed (0x%x)\n", ack);
        return;
    }
    mouse_wait_read();
    uint8_t test = inb(0x60); // 0xAA (self test passed)
    if (test != 0xAA) {
        kprintf("Mouse: self-test failed (0x%x)\n", test);
        return;
    }
    mouse_wait_read();
    uint8_t id = inb(0x60); // 0x00 (mouse ID)
    kprintf("Mouse: id 0x%x\n", id);

    // set defaults
    mouse_write(0xF6);
    mouse_wait_read();
    inb(0x60); // ACK

    // set sample rate to 40
    mouse_write(0xF3);
    mouse_wait_read();
    inb(0x60);
    mouse_write(40);
    mouse_wait_read();
    inb(0x60);

    // set resolution to 4 counts/mm
    mouse_write(0xE8);
    mouse_wait_read();
    inb(0x60); // ACK
    mouse_write(0x02);
    mouse_wait_read();
    inb(0x60); // ACK

    // set scaling to 1:1
    mouse_write(0xE6);
    mouse_wait_read();
    inb(0x60); // ACK

    // Enable data reporting
    mouse_write(0xF4);
    mouse_wait_read();
    inb(0x60); // ACK

    // Flush any remaining data
    while (inb(0x64) & 0x01)
        inb(0x60);

    idt_register_irq(MOUSE_IRQ, mouse_irq);
    mouse_ready = 1;
    kprintf("Mouse: ready.\n");
}

void mouse_set_screen(int w, int h)
{
    screen_w = w;
    screen_h = h;
    state.x = w / 2;
    state.y = h / 2;
}

mouse_state_t mouse_get(void)
{
    mouse_state_t s = state;
    return s;
}

int mouse_moved(void)
{
    if (changed)
    {
        changed = 0;
        return 1;
    }
    return 0;
}
