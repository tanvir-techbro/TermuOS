#include "terminal.h"
#include "fb.h"

// ─── Config ───────────────────────────────────────────────────────────────────

#define PADDING  4

// ─── State ────────────────────────────────────────────────────────────────────

static uint64_t _width, _height;
static int      _cols, _rows;
static int      _cx, _cy;
static uint32_t _fg, _bg;

// ─── Scroll ───────────────────────────────────────────────────────────────────

static void scroll_up(void)
{
    struct limine_framebuffer *fb = fb_get();
    if (!fb) return;

    int line = GFX_FONT_H;
    int bot  = (int)fb->height - PADDING;

    uint8_t *base = (uint8_t *)fb->address;

    for (int y = PADDING; y < bot - line; y++)
    {
        uint32_t *dst = (uint32_t *)(base + (uint64_t)y        * fb->pitch);
        uint32_t *src = (uint32_t *)(base + (uint64_t)(y+line) * fb->pitch);
        for (uint64_t x = 0; x < fb->width; x++)
            dst[x] = src[x];
    }

    gfx_fill_rect(0, bot - line, (int)fb->width, line, _bg);
}

// ─── Public API ───────────────────────────────────────────────────────────────

void terminal_init(void)
{
    _fg = 0x00FF88;
    _bg = 0x0D0D0D;
}

void terminal_set_size(uint64_t w, uint64_t h)
{
    _width  = w;
    _height = h;
    _cols   = ((int)w - PADDING * 2) / GFX_FONT_W;
    _rows   = ((int)h - PADDING * 2) / GFX_FONT_H;
    _cx     = PADDING;
    _cy     = PADDING;
}

void terminal_set_fg(uint8_t r, uint8_t g, uint8_t b)
{
    _fg = fb_colour(r, g, b);
}

void terminal_set_bg(uint8_t r, uint8_t g, uint8_t b)
{
    _bg = fb_colour(r, g, b);
    fb_clear(_bg);
    _cx = PADDING;
    _cy = PADDING;
}

void terminal_putchar(char c)
{
    if (c == '\r') {
        _cx = PADDING;
        return;
    }

    if (c == '\n') {
        _cx  = PADDING;
        _cy += GFX_FONT_H;
        if (_cy + GFX_FONT_H > (int)_height - PADDING) {
            scroll_up();
            _cy -= GFX_FONT_H;
        }
        return;
    }

    if (c == '\b') {
        if (_cx > PADDING) {
            _cx -= GFX_FONT_W;
            gfx_draw_char(_cx, _cy, ' ', _fg, _bg);
        }
        return;
    }

    if (c == '\t') {
        for (int i = 0; i < 4; i++) terminal_putchar(' ');
        return;
    }

    gfx_draw_char(_cx, _cy, c, _fg, _bg);
    _cx += GFX_FONT_W;

    if (_cx + GFX_FONT_W > (int)_width - PADDING) {
        _cx  = PADDING;
        _cy += GFX_FONT_H;
        if (_cy + GFX_FONT_H > (int)_height - PADDING) {
            scroll_up();
            _cy -= GFX_FONT_H;
        }
    }
}

void terminal_puts(const char *s)
{
    while (*s) terminal_putchar(*s++);
}

void terminal_set_size_from_current(void)
{
    _cols = ((int)_width  - PADDING * 2) / GFX_FONT_W;
    _rows = ((int)_height - PADDING * 2) / GFX_FONT_H;
    _cx   = PADDING;
    _cy   = PADDING;
}