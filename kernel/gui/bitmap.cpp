#include "bitmap.hpp"
extern "C" {
#include "../drivers/video/fb.h"
}
#include <stdint.h>

namespace Bitmap
{

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

    void draw(int x, int y, const uint8_t *data, int w, int h)
    {
        (void)w;
        (void)h;
        if (!data)
            return;
        if (data[0] != 'B' || data[1] != 'M')
            return;

        uint32_t data_off = read_u32(data, 10);
        int32_t bmp_w = read_i32(data, 18);
        int32_t bmp_h = read_i32(data, 22);
        uint16_t bpp = read_u16(data, 28);
        uint32_t comp = read_u32(data, 30);

        if (bpp != 24 || comp != 0)
            return;

        bool top_down = false;
        if (bmp_h < 0)
        {
            bmp_h = -bmp_h;
            top_down = true;
        }

        int iw = (int)bmp_w;
        int ih = (int)bmp_h;
        int row_size = ((iw * 3 + 3) / 4) * 4;

        const uint8_t *pixels = data + data_off;

        for (int row = 0; row < ih; row++)
        {
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

} // namespace Bitmap