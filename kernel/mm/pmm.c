#include "pmm.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

// ─── Bitmap allocator ────────────────────────────────────────────────────────
//
// One bit per 4KB page. 0 = free, 1 = used.
// The bitmap itself is placed at the first usable region large enough to hold it.

#define BITS_PER_ENTRY 64
#define PAGE_TO_BIT(p) ((p) / PAGE_SIZE)
#define BIT_IDX(bit) ((bit) / BITS_PER_ENTRY)
#define BIT_OFF(bit) ((bit) % BITS_PER_ENTRY)

static uint64_t *bitmap = NULL; // pointer to bitmap in physical memory
static size_t bm_size = 0;      // number of uint64_t entries in bitmap
static size_t total_pages = 0;
static size_t free_pages = 0;
static uint64_t highest_addr = 0; // highest usable physical address

// Limine gives us physical addresses directly — no HHDM needed for early boot
// We will access the bitmap via its physical address (identity-mapped by Limine)

static inline void bitmap_set(size_t bit)
{
    bitmap[BIT_IDX(bit)] |= (1ULL << BIT_OFF(bit));
}

static inline void bitmap_clear(size_t bit)
{
    bitmap[BIT_IDX(bit)] &= ~(1ULL << BIT_OFF(bit));
}

static inline int bitmap_test(size_t bit)
{
    return (bitmap[BIT_IDX(bit)] >> BIT_OFF(bit)) & 1;
}

// ─── Init ────────────────────────────────────────────────────────────────────

void pmm_init(struct limine_memmap_response *memmap)
{
    // Pass 1: find highest usable address
    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t end = e->base + e->length;
            if (end > highest_addr)
                highest_addr = end;
        }
    }

    // How many pages and how many bitmap entries we need
    size_t total_bits = highest_addr / PAGE_SIZE;
    bm_size = (total_bits + BITS_PER_ENTRY - 1) / BITS_PER_ENTRY;
    size_t bm_bytes = bm_size * sizeof(uint64_t);

    // Pass 2: find a usable region large enough to hold the bitmap
    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bm_bytes)
        {
            bitmap = (uint64_t *)e->base;
            break;
        }
    }

    if (!bitmap)
    {
        for (;;)
            __asm__ volatile("hlt");
    }

    // Mark everything as used initially
    for (size_t i = 0; i < bm_size; i++)
        bitmap[i] = 0xffffffffffffffff;

    // Pass 3: mark all USABLE pages as free
    for (uint64_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t base = e->base;
            uint64_t len = e->length;
            for (uint64_t off = 0; off < len; off += PAGE_SIZE)
            {
                bitmap_clear(PAGE_TO_BIT(base + off));
                free_pages++;
                total_pages++;
            }
        }
    }

    // Pass 4: mark the bitmap pages themselves as used
    size_t bm_pages = (bm_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < bm_pages; i++)
    {
        size_t bit = PAGE_TO_BIT((uint64_t)bitmap + i * PAGE_SIZE);
        if (!bitmap_test(bit))
        {
            bitmap_set(bit);
            free_pages--;
        }
    }
}

// ─── Alloc / Free ────────────────────────────────────────────────────────────

void *pmm_alloc(void)
{
    for (size_t i = 0; i < bm_size; i++)
    {
        if (bitmap[i] == 0xffffffffffffffff)
            continue; // all 64 pages in this entry are used

        // Find the first free bit
        for (int bit = 0; bit < BITS_PER_ENTRY; bit++)
        {
            if (!((bitmap[i] >> bit) & 1))
            {
                bitmap[i] |= (1ULL << bit);
                free_pages--;
                return (void *)((uint64_t)(i * BITS_PER_ENTRY + bit) * PAGE_SIZE);
            }
        }
    }

    kprintf("PMM: out of memory!\n");
    return NULL;
}

void pmm_free(void *addr)
{
    size_t bit = PAGE_TO_BIT((uint64_t)addr);
    if (bitmap_test(bit))
    {
        bitmap_clear(bit);
        free_pages++;
    }
}

size_t pmm_free_pages(void) { return free_pages; }
size_t pmm_total_pages(void) { return total_pages; }