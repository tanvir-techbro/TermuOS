#include "pit.h"
#include "idt.h"
#include "../../sched/scheduler.h"
#include <stdint.h>

#define PIT_CHANNEL0 0x40
#define PIT_CMD 0x43
#define PIT_BASE_HZ 1193182

volatile uint64_t timer_ticks = 0;

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" ::"a"(val), "Nd"(port));
}

static void pit_irq(registers_t *r)
{
    (void)r;
    timer_ticks++;
    scheduler_tick(r); // give the scheduler a chance to switch
}

void pit_init(uint32_t hz)
{
    uint32_t divisor = PIT_BASE_HZ / hz;

    // Channel 0, lobyte/hibyte, rate generator
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xff);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xff);

    idt_register_irq(0, pit_irq); // IRQ0 = timer
}

uint64_t pit_ticks(void) { return timer_ticks; }