#include "mouse.h"
#include "../../arch/x86_64/idt.h"
#include "../../lib/printf.h"
#include <stdint.h>

// PS/2 ports
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

#define MOUSE_IRQ   12  // IRQ12 = PS/2 mouse

// packet state
static uint8_t packet[3];
static int packet_idx = 0;
static volatile mouse_state_t state = {0};
static volatile int changed = 0;

static int screen_w = 1280;
static int screen_h = 800;

// I/O helpers
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0,%1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}

static void ps2_wait_write(void)
{
    int timeout = 100000;
    while ((inb(PS2_STATUS) & 0x02) && timeout--);
}

static void ps2_wait_read(void)
{
    int timeout = 100000;
    while (!(inb(PS2_STATUS) & 0x01) && timeout--);
}

static void ps2_write_cmd(uint8_t cmd)
{
    ps2_wait_write();
    outb(PS2_CMD, cmd);
}

static void ps2_write_data(uint8_t data)
{
    ps2_wait_write();
    outb(PS2_DATA, data);
}

static uint8_t ps2_read_data(void)
{
    ps2_wait_read();
    return inb(PS2_DATA);
}

static void mouse_write(uint8_t cmd)
{
    ps2_write_cmd(0x04); // route next byte to mouse
    ps2_write_data(cmd);
    ps2_read_data(); // read ACK
}

// IRQ handler
static void mouse_irq(registers_t *r)
{
    (void)r;

    // check output buffer full and aux data bit
    uint8_t status = inb(PS2_STATUS);
    if (!(status & 0x01)) return;
    if (!(status & 0x20)) return; // not from mouse

    uint8_t byte = inb(PS2_DATA);

    packet[packet_idx++] = byte;

    if (packet_idx == 1)
    {
        // first byte: must have bit 3 set (always-1 bit)
        if (!(byte & 0x08)) { packet_idx = 0; return; }
    }

    if (packet_idx < 3) return;
    packet_idx = 0;

    // parse 3-byte packet
    uint8_t flags = packet[0];
    int8_t mx = (int8_t)packet[1];
    int8_t my = (int8_t)packet[2];

    // apply sign extension from overflow bits
    if (flags & 0x10) mx = (int8_t)(packet[1] | 0x100 - 256);
    if (flags & 0x20) my = (int8_t)(packet[2] | 0x100 - 256);

    // discard on overflow
    if (flags & 0xC0) return;

    state.dx = mx;
    state.dy = -my; // Y is inverted
    state.x += mx;
    state.y -= my;

    // clamp
    if (state.x < 0) state.x = 0;
    if (state.y < 0) state.y = 0;
    if (state.x >= screen_w) state.x = screen_w - 1;
    if (state.y >= screen_h) state.y = screen_h - 1;

    state.left = (flags & 0x01) ? 1 : 0;
    state.right = (flags & 0x02) ? 1 : 0;
    state.middle = (flags & 0x04) ? 1 : 0;

    changed = 1;
}

// init
void mouse_init(void)
{
    // enable auxiliary mouse device
    ps2_write_cmd(0xA8);

    // enable interrupts for mouse (set bit 1 of compaq status byte)
    ps2_write_cmd(0x20); // read compaq status
    uint8_t status = ps2_read_data();
    status |= 0x02; // enable IRQ12
    status &= ~0x20; // enable mouse clock
    ps2_write_cmd(0x60); // write compaq status
    ps2_write_data(status);

    // reset mouse
    mouse_write(0xFF);

    // set default settings
    mouse_write(0xF6); // set defaults
    mouse_write(0xF4); // enable data reporting

    // Start at centre of screen
    state.x = screen_w / 2;
    state.y = screen_h / 2;

    idt_register_irq(MOUSE_IRQ, mouse_irq);
    kprintf("Mouse: PS/2 ready.\n");
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
    return (mouse_state_t)
    {
        .x = state.x, .y = state.y,
        .dx = state.dx, .dy = state.dy,
        .left = state.left, .right = state.right, .middle = state.middle
    };
}

int mouse_moved(void)
{
    if (changed) { changed = 0; return 1; }
    return 0;
}