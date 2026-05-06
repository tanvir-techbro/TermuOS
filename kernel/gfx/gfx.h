#pragma once
#include <stdint.h>
#include <stddef.h>

// colour helpers
#define RGB(r, g, b) gfx_rgb(r, g, b)
#define RGBA(r, g, b, a) gfx_rgba(r, g, b, a)

// common colours
#define GFX_BLACK RGB(0, 0, 0)
#define GFX_WHITE RGB(255, 255, 255)
#define GFX_RED RGB(255, 0, 0)
#define GFX_GREEN RGB(0, 255, 0)
#define GFX_BLUE RGB(0, 0, 255)
#define GFX_YELLOW RGB(255, 255, 0)
#define GFX_CYAN RGB(0, 255, 255)
#define GFX_MAGENTA RGB(255, 0, 255)
#define GFX_GREY RGB(128, 128, 128)
#define GFX_ORANGE RGB(255, 165, 0)

typedef struct
{
    int x, y, w, h;
} gfx_rect_t;

// init - call after fb_init()
void gfx_init(void);

uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t gfx_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

// screen info
int gfx_width(void);
int gfx_height(void);

// primitives
void gfx_clear(uint32_t colour);
void gfx_pixel(int x, int y, uint32_t colour);
void gfx_hline(int x, int y, int w, uint32_t colour);
void gfx_vline(int x, int y, int h, uint32_t colour);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t colour);
void gfx_rect(int x, int y, int w, int h, uint32_t colour);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t colour);
void gfx_circle(int cx, int cy, int r, uint32_t colour);
void gfx_fill_circle(int cx, int cy, int r, uint32_t colour);
void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t colour);
void gfx_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t colour);
void gfx_rounded_rect(int x, int y, int w, int h, int r, uint32_t colour);
void gfx_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t colour);

// text
void gfx_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void gfx_textf(int x, int y, uint32_t fg, uint32_t bg, const char *fmt, ...);

// clipping
void gfx_set_clip(int x, int y, int w, int h);
void gfx_clear_clip(void);

// ─── Cursor ───────────────────────────────────────────────────────────────────
void gfx_cursor_draw(int x, int y);