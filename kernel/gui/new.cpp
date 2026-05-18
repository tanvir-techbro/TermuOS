extern "C" {
#include "../mm/heap.h"
}
#include <stddef.h>

extern "C" {
    void __cxa_atexit(void (*)(void *), void *, void *) {}
    void __dso_handle() {}
    void atexit(void (*)(void)) {}
}

// Regular new/delete
void *operator new(size_t size)              { return kmalloc(size); }
void *operator new[](size_t size)            { return kmalloc(size); }
void  operator delete(void *p)    noexcept   { kfree(p); }
void  operator delete[](void *p)  noexcept   { kfree(p); }
void  operator delete(void *p, size_t) noexcept  { kfree(p); }
void  operator delete[](void *p, size_t) noexcept { kfree(p); }

// Placement new — constructs in existing storage, no allocation
void *operator new(size_t, void *ptr)   noexcept { return ptr; }
void *operator new[](size_t, void *ptr) noexcept { return ptr; }
