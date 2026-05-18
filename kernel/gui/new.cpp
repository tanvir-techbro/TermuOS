#include "../mm/heap.h"
#include <stddef.h>

void *operator new(size_t size) { return kmalloc(size); }
void *operator new[](size_t size) { return kmalloc(size); }
void operator delete(void *p) noexcept { kfree(p); }
void operator delete[](void *p) noexcept { kfree(p); }
void operator delete(void *p, size_t) noexcept { kfree(p); }
void operator delete[](void *p, size_t) noexcept { kfree(p); }
