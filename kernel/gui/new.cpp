extern "C" {
#include "../mm/heap.h"
}
#include <stddef.h>
#include <stdint.h>

extern "C" {
    void atexit(void (*)(void)) {}
    void __dso_handle() {}
    void __cxa_atexit(void (*)(void *), void *, void *) {}
    void __cxa_guard_abort(void *) {}

    // Guard: byte 0 = initialized flag, byte 1 = in-progress flag
    int __cxa_guard_acquire(uint8_t *guard)
    {
        if (guard[0]) return 0;  // already initialized
        guard[1] = 1;            // mark in progress
        return 1;                // tell compiler to run initializer
    }

    void __cxa_guard_release(uint8_t *guard)
    {
        guard[0] = 1;  // mark initialized
        guard[1] = 0;  // clear in progress
    }
}

extern "C" void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    while(n--) *p++ = (uint8_t)c;
    return s;
}

// Regular new/delete
void *operator new(size_t size)              { return kmalloc(size); }
void *operator new[](size_t size)            { return kmalloc(size); }
void  operator delete(void *p)    noexcept   { kfree(p); }
void  operator delete[](void *p)  noexcept   { kfree(p); }
void  operator delete(void *p, size_t) noexcept  { kfree(p); }
void  operator delete[](void *p, size_t) noexcept { kfree(p); }

// Placement new
void *operator new(size_t, void *ptr)   noexcept { return ptr; }
void *operator new[](size_t, void *ptr) noexcept { return ptr; }
