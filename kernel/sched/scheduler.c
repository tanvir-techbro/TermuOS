#include "scheduler.h"
#include "../mm/heap.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

thread_t threads[MAX_THREADS];
static int current = 0;
static int initialized = 0;

extern void context_switch(uint64_t *old_rsp, uint64_t new_rsp);

void scheduler_init(void)
{
    for (int i = 0; i < MAX_THREADS; i++)
        threads[i].state = THREAD_DEAD;

    threads[0].id = 0;
    threads[0].state = THREAD_RUNNING;
    threads[0].stack_top = 0;
    __builtin_memcpy(threads[0].name, "idle", 5);

    current = 0;
    initialized = 1;
    kprintf("Scheduler: init.\n");
}

thread_t *thread_create(const char *name, void (*entry)(void))
{
    int slot = -1;
    for (int i = 1; i < MAX_THREADS; i++)
        if (threads[i].state == THREAD_DEAD)
        {
            slot = i;
            break;
        }
    if (slot < 0)
    {
        kprintf("scheduler: no free slots\n");
        return NULL;
    }

    uint8_t *stack = (uint8_t *)kmalloc(STACK_SIZE);
    if (!stack)
    {
        kprintf("scheduler: no memory\n");
        return NULL;
    }

    thread_t *t = &threads[slot];
    t->id = slot;
    t->state = THREAD_READY;
    t->stack_top = (uint64_t)(stack + STACK_SIZE);

    int i = 0;
    while (name[i] && i < 31)
    {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    uint64_t *sp = (uint64_t *)t->stack_top;
    *--sp = (uint64_t)thread_exit;
    *--sp = (uint64_t)entry;
    *--sp = 0; // rbp
    *--sp = 0; // rbx
    *--sp = 0; // r12
    *--sp = 0; // r13
    *--sp = 0; // r14
    *--sp = 0; // r15
    t->rsp = (uint64_t)sp;

    kprintf("Scheduler: created thread %d '%s'\n", slot, t->name);
    return t;
}

void thread_exit(void)
{
    __asm__ volatile("cli");
    threads[current].state = THREAD_DEAD;
    __asm__ volatile("sti");
    scheduler_yield();
    for (;;)
        __asm__ volatile("hlt");
}

void thread_block(void)
{
    threads[current].state = THREAD_BLOCKED;
    scheduler_yield();
}

void thread_unblock(thread_t *t)
{
    __asm__ volatile("cli");
    if (t->state == THREAD_BLOCKED)
        t->state = THREAD_READY;
    __asm__ volatile("sti");
}

thread_t *thread_current(void) { return &threads[current]; }

static int next_thread(void)
{
    for (int i = 1; i <= MAX_THREADS; i++)
    {
        int idx = (current + i) % MAX_THREADS;
        if (threads[idx].state == THREAD_READY)
            return idx;
    }
    return -1;
}

void scheduler_yield(void)
{
    if (!initialized)
        return;
    __asm__ volatile("cli");

    int next = next_thread();
    if (next < 0)
    {
        __asm__ volatile("sti");
        return;
    }

    int prev = current;
    if (threads[prev].state == THREAD_RUNNING)
        threads[prev].state = THREAD_READY;
    threads[next].state = THREAD_RUNNING;
    current = next;

    __asm__ volatile("sti");
    context_switch(&threads[prev].rsp, threads[next].rsp);
}

// Called from PIT IRQ — only switch away from idle thread
void scheduler_tick(registers_t *r)
{
    (void)r;
    // Shell runs in main thread — no preemption needed
}