#include "gfx.h"
#include "../drivers/video/fb.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

// backbuffer
static uint32_t *_backbuffer = NULL;

// state
static int _w, _h;
static uint8_t _rs, _gs, _bs; // channel shifts from fb

// clip rectangle (default = full screen)
static int _cx = 0, _cy = 0, _cw = 0, _ch = 0;
static int _clipping = 0;

// font same 8x8 as terminal
static const uint8_t font8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00}, // '!'
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // '"'
    {0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00}, // '#'
    {0x0c, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x0c, 0x00}, // '$'
    {0x00, 0x63, 0x33, 0x18, 0x0c, 0x66, 0x63, 0x00}, // '%'
    {0x1c, 0x36, 0x1c, 0x6e, 0x3b, 0x33, 0x6e, 0x00}, // '&'
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // '''
    {0x18, 0x0c, 0x06, 0x06, 0x06, 0x0c, 0x18, 0x00}, // '('
    {0x06, 0x0c, 0x18, 0x18, 0x18, 0x0c, 0x06, 0x00}, // ')'
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00}, // '*'
    {0x00, 0x0c, 0x0c, 0x3f, 0x0c, 0x0c, 0x00, 0x00}, // '+'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x06}, // ','
    {0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00}, // '-'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x00}, // '.'
    {0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x01, 0x00}, // '/'
    {0x1e, 0x33, 0x3b, 0x37, 0x33, 0x33, 0x1e, 0x00}, // '0'
    {0x0c, 0x0e, 0x0c, 0x0c, 0x0c, 0x0c, 0x3f, 0x00}, // '1'
    {0x1e, 0x33, 0x30, 0x1c, 0x06, 0x33, 0x3f, 0x00}, // '2'
    {0x1e, 0x33, 0x30, 0x1c, 0x30, 0x33, 0x1e, 0x00}, // '3'
    {0x38, 0x3c, 0x36, 0x33, 0x7f, 0x30, 0x78, 0x00}, // '4'
    {0x3f, 0x03, 0x1f, 0x30, 0x30, 0x33, 0x1e, 0x00}, // '5'
    {0x1c, 0x06, 0x03, 0x1f, 0x33, 0x33, 0x1e, 0x00}, // '6'
    {0x3f, 0x33, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x00}, // '7'
    {0x1e, 0x33, 0x33, 0x1e, 0x33, 0x33, 0x1e, 0x00}, // '8'
    {0x1e, 0x33, 0x33, 0x3e, 0x30, 0x18, 0x0e, 0x00}, // '9'
    {0x00, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x0c, 0x00}, // ':'
    {0x00, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x0c, 0x06}, // ';'
    {0x18, 0x0c, 0x06, 0x03, 0x06, 0x0c, 0x18, 0x00}, // '<'
    {0x00, 0x00, 0x3f, 0x00, 0x00, 0x3f, 0x00, 0x00}, // '='
    {0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00}, // '>'
    {0x1e, 0x33, 0x30, 0x18, 0x0c, 0x00, 0x0c, 0x00}, // '?'
    {0x3e, 0x63, 0x7b, 0x7b, 0x7b, 0x03, 0x1e, 0x00}, // '@'
    {0x0c, 0x1e, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x00}, // 'A'
    {0x3f, 0x66, 0x66, 0x3e, 0x66, 0x66, 0x3f, 0x00}, // 'B'
    {0x3c, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3c, 0x00}, // 'C'
    {0x1f, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1f, 0x00}, // 'D'
    {0x7f, 0x46, 0x16, 0x1e, 0x16, 0x46, 0x7f, 0x00}, // 'E'
    {0x7f, 0x46, 0x16, 0x1e, 0x16, 0x06, 0x0f, 0x00}, // 'F'
    {0x3c, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7c, 0x00}, // 'G'
    {0x33, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x33, 0x00}, // 'H'
    {0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00}, // 'I'
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1e, 0x00}, // 'J'
    {0x67, 0x66, 0x36, 0x1e, 0x36, 0x66, 0x67, 0x00}, // 'K'
    {0x0f, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7f, 0x00}, // 'L'
    {0x63, 0x77, 0x7f, 0x7f, 0x6b, 0x63, 0x63, 0x00}, // 'M'
    {0x63, 0x67, 0x6f, 0x7b, 0x73, 0x63, 0x63, 0x00}, // 'N'
    {0x1c, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1c, 0x00}, // 'O'
    {0x3f, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x0f, 0x00}, // 'P'
    {0x1e, 0x33, 0x33, 0x33, 0x3b, 0x1e, 0x38, 0x00}, // 'Q'
    {0x3f, 0x66, 0x66, 0x3e, 0x36, 0x66, 0x67, 0x00}, // 'R'
    {0x1e, 0x33, 0x07, 0x0e, 0x38, 0x33, 0x1e, 0x00}, // 'S'
    {0x3f, 0x2d, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00}, // 'T'
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3f, 0x00}, // 'U'
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x00}, // 'V'
    {0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00}, // 'W'
    {0x63, 0x63, 0x36, 0x1c, 0x1c, 0x36, 0x63, 0x00}, // 'X'
    {0x33, 0x33, 0x33, 0x1e, 0x0c, 0x0c, 0x1e, 0x00}, // 'Y'
    {0x7f, 0x63, 0x31, 0x18, 0x4c, 0x66, 0x7f, 0x00}, // 'Z'
    {0x1e, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1e, 0x00}, // '['
    {0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x40, 0x00}, // '\'
    {0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1e, 0x00}, // ']'
    {0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // '^'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff}, // '_'
    {0x0c, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // '`'
    {0x00, 0x00, 0x1e, 0x30, 0x3e, 0x33, 0x6e, 0x00}, // 'a'
    {0x07, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x3b, 0x00}, // 'b'
    {0x00, 0x00, 0x1e, 0x33, 0x03, 0x33, 0x1e, 0x00}, // 'c'
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6e, 0x00}, // 'd'
    {0x00, 0x00, 0x1e, 0x33, 0x3f, 0x03, 0x1e, 0x00}, // 'e'
    {0x1c, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0f, 0x00}, // 'f'
    {0x00, 0x00, 0x6e, 0x33, 0x33, 0x3e, 0x30, 0x1f}, // 'g'
    {0x07, 0x06, 0x36, 0x6e, 0x66, 0x66, 0x67, 0x00}, // 'h'
    {0x0c, 0x00, 0x0e, 0x0c, 0x0c, 0x0c, 0x1e, 0x00}, // 'i'
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1e}, // 'j'
    {0x07, 0x06, 0x66, 0x36, 0x1e, 0x36, 0x67, 0x00}, // 'k'
    {0x0e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00}, // 'l'
    {0x00, 0x00, 0x33, 0x7f, 0x7f, 0x6b, 0x63, 0x00}, // 'm'
    {0x00, 0x00, 0x1f, 0x33, 0x33, 0x33, 0x33, 0x00}, // 'n'
    {0x00, 0x00, 0x1e, 0x33, 0x33, 0x33, 0x1e, 0x00}, // 'o'
    {0x00, 0x00, 0x3b, 0x66, 0x66, 0x3e, 0x06, 0x0f}, // 'p'
    {0x00, 0x00, 0x6e, 0x33, 0x33, 0x3e, 0x30, 0x78}, // 'q'
    {0x00, 0x00, 0x3b, 0x6e, 0x66, 0x06, 0x0f, 0x00}, // 'r'
    {0x00, 0x00, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x00}, // 's'
    {0x08, 0x0c, 0x3e, 0x0c, 0x0c, 0x2c, 0x18, 0x00}, // 't'
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6e, 0x00}, // 'u'
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x00}, // 'v'
    {0x00, 0x00, 0x63, 0x6b, 0x7f, 0x7f, 0x36, 0x00}, // 'w'
    {0x00, 0x00, 0x63, 0x36, 0x1c, 0x36, 0x63, 0x00}, // 'x'
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3e, 0x30, 0x1f}, // 'y'
    {0x00, 0x00, 0x3f, 0x19, 0x0c, 0x26, 0x3f, 0x00}, // 'z'
    {0x38, 0x0c, 0x0c, 0x07, 0x0c, 0x0c, 0x38, 0x00}, // '{'
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // '|'
    {0x07, 0x0c, 0x0c, 0x38, 0x0c, 0x0c, 0x07, 0x00}, // '}'
    {0x6e, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // '~'
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // DEL (solid block)
};

// init
extern uint64_t fb_width_val;
extern uint64_t fb_height_val;
extern uint8_t fb_red_shift;
extern uint8_t fb_green_shift;
extern uint8_t fb_blue_shift;

static uint32_t (*_pixel_fn)(int, int, uint32_t) = NULL;

static uint8_t *_fb_base;
static uint64_t _fb_pitch;
static uint8_t _fb_rs, _fb_gs, _fb_bs;

void gfx_init(void)
{
    struct limine_framebuffer *fb = fb_get();

    if (!fb)
        return; // or panic

    _w = fb->width;
    _h = fb->height;
    _fb_base = (uint8_t *)fb->address;
    _fb_pitch = fb->pitch;
    _fb_rs = fb->red_mask_shift;
    _fb_gs = fb->green_mask_shift;
    _fb_bs = fb->blue_mask_shift;

    _cx = 0;
    _cy = 0;
    _cw = _w;
    _ch = _h;
    _clipping = 0;
}

int gfx_width(void) { return _w; }
int gfx_height(void) { return _h; }

// colour
uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << _fb_rs) | ((uint32_t)g << _fb_gs) | ((uint32_t)b << _fb_bs);
}

uint32_t gfx_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    // simple alpha blend against black bg
    r = (r * a) / 255;
    g = (g * a) / 255;
    b = (b * a) / 255;
    return gfx_rgb(r, g, b);
}

// pixel (with clipping)
static inline int in_clip(int x, int y)
{
    if (!_clipping)
        return (x >= 0 && x < _w && y >= 0 && y < _h);
    return (x >= _cx && x < _cx + _cw && y >= _cy && y < _cy + _ch);
}

static inline void put_pixel(int x, int y, uint32_t colour)
{
    uint32_t *row = (uint32_t *)(_fb_base + y * _fb_pitch);
    row[x] = colour;
}

void gfx_pixel(int x, int y, uint32_t colour)
{
    if (in_clip(x, y))
        put_pixel(x, y, colour);
}

// clear
void gfx_clear(uint32_t colour)
{
    for (int y = 0; y < _h; y++)
    {
        uint32_t *row = (uint32_t *)(_fb_base + y * _fb_pitch);
        for (int x = 0; x < _w; x++)
            row[x] = colour;
    }
}

// lines
void gfx_hline(int x, int y, int w, uint32_t colour)
{
    for (int i = x; i < x + w; i++)
        gfx_pixel(i, y, colour);
}

void gfx_vline(int x, int y, int h, uint32_t colour)
{
    for (int i = y; i < y + h; i++)
        gfx_pixel(x, i, colour);
}

// bresenham line
void gfx_line(int x0, int y0, int x1, int y1, uint32_t colour)
{
    int dx = (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        gfx_pixel(x0, y0, colour);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

// rectangles
void gfx_rect(int x, int y, int w, int h, uint32_t colour)
{
    gfx_hline(x, y, w, colour);
    gfx_hline(x, y + h - 1, w, colour);
    gfx_vline(x, y, h, colour);
    gfx_vline(x + w - 1, y, h, colour);
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t colour)
{
    for (int row = y; row < y + h; row++)
        gfx_hline(x, row, w, colour);
}

// circles
void gfx_circle(int cx, int cy, int r, uint32_t colour)
{
    int x = 0, y = r, d = 1 - r;
    while (x <= y)
    {
        gfx_pixel(cx + x, cy + y, colour);
        gfx_pixel(cx - x, cy + y, colour);
        gfx_pixel(cx + x, cy - y, colour);
        gfx_pixel(cx - x, cy - y, colour);
        gfx_pixel(cx + y, cy + x, colour);
        gfx_pixel(cx - y, cy + x, colour);
        gfx_pixel(cx + y, cy - x, colour);
        gfx_pixel(cx - y, cy - x, colour);
        if (d < 0)
            d += 2 * x + 3;
        else
        {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void gfx_fill_circle(int cx, int cy, int r, uint32_t colour)
{
    int x = 0, y = r, d = 1 - r;
    while (x <= y)
    {
        gfx_hline(cx - x, cy + y, 2 * x + 1, colour);
        gfx_hline(cx - x, cy - y, 2 * x + 1, colour);
        gfx_hline(cx - y, cy + x, 2 * y + 1, colour);
        gfx_hline(cx - y, cy - x, 2 * x + 1, colour);
        if (d < 0)
            d += 2 * x + 3;
        else
        {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

// triangles
void gfx_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t colour)
{
    gfx_line(x0, y0, x1, y1, colour);
    gfx_line(x1, y1, x2, y2, colour);
    gfx_line(x2, y2, x0, y0, colour);
}

static void sort3(int *a, int *b, int *x1, int *y1, int *x2, int *y2)
{
    // sort vertices by y
    if (*a > *b)
    {
        int t = *a;
        *a = *b;
        *b = t;
        t = *x1;
        *x1 = *x2;
        *x2 = t;
        t = *y1;
        *y1 = *y2;
        *y2 = t;
    }
}

void gfx_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t colour)
{
    // sort by y: y0 <= y1 <= y2
    if (y0 > y1)
    {
        int t;
        t = x0;
        x0 = x1;
        x1 = t;
        t = y0;
        y0 = y1;
        y1 = t;
    }
    if (y0 > y2)
    {
        int t;
        t = x0;
        x0 = x2;
        x2 = t;
        t = y0;
        y0 = y2;
        y2 = t;
    }
    if (y1 > y2)
    {
        int t;
        t = x1;
        x1 = x2;
        x2 = t;
        t = y1;
        y1 = y2;
        y2 = t;
    }
    (void)sort3;

    int total_h = y2 - y0;
    if (!total_h)
        return;

    for (int y = y0; y <= y2; y++)
    {
        int upper = (y < y1);
        int seg_h = upper ? (y1 - y0 + 1) : (y2 - y1 + 1);
        if (!seg_h)
            continue;

        int alpha = (y - y0) * 256 / total_h;
        int beta = upper ? ((y - y0) * 256 / (y1 - y0 + 1)) : ((y - y1) * 256 / (y2 - y1 + 1));

        int ax = x0 + (x2 - x0) * alpha / 256;
        int bx = upper ? (x0 + (x1 - x0) * beta / 256) : (x1 + (x2 - x1) * beta / 256);

        if (ax > bx)
        {
            int t = ax;
            ax = bx;
            bx = t;
        }
        gfx_hline(ax, y, bx - ax + 1, colour);
    }
}

// rounded rectangles
void gfx_rounded_rect(int x, int y, int w, int h, int r, uint32_t colour)
{
    gfx_hline(x + r, y, w - 2 * r, colour);
    gfx_hline(x + r, y + h - 1, w - 2 * r, colour);
    gfx_vline(x, y + r, h - 2 * r, colour);
    gfx_vline(x + w - 1, y + r, h - 2 * r, colour);

    // corners using circle arcs
    int cx, cy, px = 0, py = r, d = 1 - r;
    while (px <= py)
    {
        // top left
        cx = x + r;
        cy = y + r;
        gfx_pixel(cx - px, cy - py, colour);
        gfx_pixel(cx - py, cy - px, colour);
        // top right
        cx = x + w - 1 - r;
        gfx_pixel(cx + px, cy - py, colour);
        gfx_pixel(cx + py, cy - px, colour);
        // bottom left
        cx = x + r;
        cy = y + h - 1 - r;
        gfx_pixel(cx - px, cy + py, colour);
        gfx_pixel(cx - py, cy + px, colour);
        // bottom right
        cx = x + w - 1 - r;
        gfx_pixel(cx + px, cy + py, colour);
        gfx_pixel(cx + py, cy + px, colour);

        if (d < 0)
            d += 2 * px + 3;
        else
        {
            d += 2 * (px - py) + 5;
            py--;
        }
        px++;
    }
}

void gfx_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t colour)
{
    // fill center + top/bottom strips
    gfx_fill_rect(x + r, y, w - 2 * r, h, colour);
    gfx_fill_rect(x, y + r, r, h - 2 * r, colour);
    gfx_fill_rect(x + w - r, y + r, r, h - 2 * r, colour);

    // fill corner circles
    gfx_fill_circle(x + r, y + r, r, colour);
    gfx_fill_circle(x + w - 1 - r, y + r, r, colour);
    gfx_fill_circle(x + r, y + h - 1 - r, r, colour);
    gfx_fill_circle(x + w - 1 - r, y + h - 1 - r, r, colour);
}

// text
void gfx_char(int x, int y, char c, uint32_t fg, uint32_t bg)
{
    if (c < 32 || c > 127)
        c = '?';
    const uint8_t *glyph = font8x8[c - 32];
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            uint32_t colour = (glyph[row] & (1 << col)) ? fg : bg;
            gfx_pixel(x + col, y + row, colour);
        }
    }
}

void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg)
{
    int cx = x;
    while (*s)
    {
        if (*s == '\n')
        {
            cx = x;
            y += 9;
            s++;
            continue;
        }
        gfx_char(cx, y, *s++, fg, bg);
        cx += 8;
    }
}

void gfx_textf(int x, int y, uint32_t fg, uint32_t bg, const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    // simple vsnprintf-like formatting
    int i = 0;
    for (; *fmt && i < 255; fmt++)
    {
        if (*fmt != '%')
        {
            buf[i++] = *fmt;
            continue;
        }
        fmt++;
        if (*fmt == 's')
        {
            const char *s = va_arg(args, const char *);
            while (*s && i < 255)
                buf[i++] = *s++;
        }
        else if (*fmt == 'd')
        {
            int v = va_arg(args, int);
            if (v < 0)
            {
                buf[i++] = '-';
                v = -v;
            }
            char tmp[16];
            int ti = 0;
            if (!v)
                tmp[ti++] = '0';
            while (v)
            {
                tmp[ti++] = '0' + v % 10;
                v /= 10;
            }
            while (ti--)
                buf[i++] = tmp[ti + 1];
        }
        else if (*fmt == 'u')
        {
            unsigned v = va_arg(args, unsigned);
            char tmp[16];
            int ti = 0;
            if (!v)
                tmp[ti++] = '0';
            while (v)
            {
                tmp[ti++] = '0' + v % 10;
                v /= 10;
            }
            while (ti--)
                buf[i++] = tmp[ti + 1];
        }
        else if (*fmt == 'c')
        {
            buf[i++] = (char)va_arg(args, int);
        }
        else
        {
            buf[i++] = '%';
            buf[i++] = *fmt;
        }
    }
    buf[i] = 0;
    va_end(args);
    gfx_text(x, y, buf, fg, bg);
}

// clipping
void gfx_set_clip(int x, int y, int w, int h)
{
    _cx = x;
    _cy = y;
    _cw = w;
    _ch = h;
    _clipping = 1;
}

void gfx_clear_clip(void) { _clipping = 0; }

// ─── Cursor ───────────────────────────────────────────────────────────────────
// Classic arrow cursor — 12x19 pixels
// We save/restore the pixels underneath

#define CURSOR_W 12
#define CURSOR_H 19

static uint32_t cursor_save[CURSOR_W * CURSOR_H];

// 1 = draw, 0 = transparent
static const uint8_t cursor_shape[CURSOR_H][CURSOR_W] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0},
    {1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1},
    {1, 2, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0},
    {1, 2, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {1, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
};

void gfx_cursor_draw(int x, int y)
{
    uint32_t white = gfx_rgb(255, 255, 255);
    uint32_t shadow = gfx_rgb(0, 0, 0);

    for (int row = 0; row < CURSOR_H; row++)
    {
        for (int col = 0; col < CURSOR_W; col++)
        {
            int px = x + col;
            int py = y + row;

            if (px < 0 || px >= _w || py < 0 || py >= _h)
                continue;

            uint8_t v = cursor_shape[row][col];

            if (v == 1)
                put_pixel(px, py, white);
            else if (v == 2)
                put_pixel(px, py, shadow);
        }
    }
}