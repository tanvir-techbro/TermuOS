#pragma once
#include <stdint.h>
#include <limine.h>

void fb_init(struct limine_framebuffer *fb);
void fb_clear(uint32_t colour);
uint32_t fb_colour(uint8_t r, uint8_t g, uint8_t b);
void fb_putpixel(uint64_t x, uint64_t y, uint32_t colour);