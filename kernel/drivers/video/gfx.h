#pragma once
#include <stdint.h>

void gfx_fill_rect(int x, int y, int w, int h, uint32_t colour);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t colour);
void gfx_draw_hline(int x, int y, int len, uint32_t colour);
void gfx_draw_vline(int x, int y, int len, uint32_t colour);

void gfx_draw_char(int x, int y, char c, uint32_t fb, uint32_t bg);

void gfx_draw_string(int x, int y, const char *s, uint32_t fb, uint32_t bg);

#define GFX_FONT_W 8
#define GFX_FONT_H 16
