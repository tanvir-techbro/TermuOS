#include "fb.h"

static struct limine_framebuffer *_fb = 0;

void fb_init(struct limine_framebuffer *fb)
{
    _fb = fb;
}

struct limine_framebuffer *fb_get(void)
{
    return _fb;
}

uint32_t fb_colour(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << _fb->red_mask_shift) | ((uint32_t)g << _fb->green_mask_shift) | ((uint32_t)b << _fb->blue_mask_shift);
}

void fb_putpixel(uint64_t x, uint64_t y, uint32_t colour)
{
    uint8_t *base = (uint8_t *)_fb->address;
    uint32_t *row = (uint32_t *)(base + y * _fb->pitch);
    row[x] = colour;
}

void fb_clear(uint32_t colour)
{
    uint8_t *base = (uint8_t *)_fb->address;
    for (uint64_t y = 0; y < _fb->height; y++)
    {
        uint32_t *row = (uint32_t *)(base + y * _fb->pitch);
        for (uint64_t x = 0; x < _fb->width; x++)
            row[x] = colour;
    }
}