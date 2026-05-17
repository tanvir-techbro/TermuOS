#include "bitmap.h"
#include "../drivers/video/fb.h"
#include <stdint.h>

// ─── BMP header offsets (little-endian) ──────────────────────────────────────

#define BMP_OFF_DATAOFFSET 10  // uint32 — offset to pixel data
#define BMP_OFF_WIDTH 18       // int32  — image width
#define BMP_OFF_HEIGHT 22      // int32  — image height (negative = top-down)
#define BMP_OFF_BPP 28         // uint16 — bits per pixel
#define BMP_OFF_COMPRESSION 30 // uint32 — compression type (0 = none)

static inline uint32_t read_u32(const uint8_t *d, int off)
{
    return (uint32_t)d[off] | ((uint32_t)d[off + 1] << 8) | ((uint32_t)d[off + 2] << 16) | ((uint32_t)d[off + 3] << 24);
}

static inline int32_t read_i32(const uint8_t *d, int off)
{
    return (int32_t)read_u32(d, off);
}

static inline uint16_t read_u16(const uint8_t *d, int off)
{
    return (uint16_t)(d[off] | (d[off + 1] << 8));
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void bmp_draw(int x, int y, const uint8_t *data, int w, int h)
{
    if (!data)
        return;

    // Validate BMP signature
    if (data[0] != 'B' || data[1] != 'M')
        return;

    uint32_t data_off = read_u32(data, BMP_OFF_DATAOFFSET);
    int32_t bmp_w = read_i32(data, BMP_OFF_WIDTH);
    int32_t bmp_h = read_i32(data, BMP_OFF_HEIGHT);
    uint16_t bpp = read_u16(data, BMP_OFF_BPP);
    uint32_t comp = read_u32(data, BMP_OFF_COMPRESSION);

    // Only support 24-bit uncompressed
    if (bpp != 24 || comp != 0)
        return;

    int top_down = 0;
    if (bmp_h < 0)
    {
        bmp_h = -bmp_h;
        top_down = 1;
    }

    // Use actual BMP dimensions, ignore w/h hints
    int iw = (int)bmp_w;
    int ih = (int)bmp_h;

    // Row size is padded to 4-byte boundary
    int row_size = ((iw * 3 + 3) / 4) * 4;

    const uint8_t *pixels = data + data_off;

    for (int row = 0; row < ih; row++)
    {
        // BMP is bottom-up unless top_down flag is set
        int src_row = top_down ? row : (ih - 1 - row);
        int dst_y = y + row;

        const uint8_t *src = pixels + src_row * row_size;

        for (int col = 0; col < iw; col++)
        {
            uint8_t b = src[col * 3 + 0];
            uint8_t g = src[col * 3 + 1];
            uint8_t r = src[col * 3 + 2];
            fb_putpixel(x + col, dst_y, fb_colour(r, g, b));
        }
    }
}