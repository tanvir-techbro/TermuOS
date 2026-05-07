#include "bitmap.h"
#include "gfx.h"
#include "../mm/heap.h"
#include <stdint.h>
#include <stddef.h>

bitmap_t *bmp_load(const uint8_t *data)
{
    bmp_file_header_t *file = (bmp_file_header_t *)data;

    if (file->type != 0x4D42)
        return NULL;

    bmp_info_header_t *info =
        (bmp_info_header_t *)(data + sizeof(bmp_file_header_t));

    if (info->compression != 0)
        return NULL;

    if (info->bpp != 24 && info->bpp != 32)
        return NULL;

    bitmap_t *bmp = kmalloc(sizeof(bitmap_t));

    bmp->width = info->width;
    bmp->height = (info->height < 0) ? -info->height : info->height;

    bmp->pixels = kmalloc(bmp->width * bmp->height * sizeof(uint32_t));

    const uint8_t *pixels = data + file->offset;

    int top_down = (info->height < 0);

    if (info->bpp == 24)
    {
        int row_size = ((bmp->width * 3 + 3) & ~3);

        for (int y = 0; y < bmp->height; y++)
        {
            int src_y = top_down ? y : (bmp->height - 1 - y);

            const uint8_t *row = pixels + src_y * row_size;

            for (int x = 0; x < bmp->width; x++)
            {
                uint8_t b = row[x * 3 + 0];
                uint8_t g = row[x * 3 + 1];
                uint8_t r = row[x * 3 + 2];

                bmp->pixels[y * bmp->width + x] =
                    gfx_rgb(r, g, b);
            }
        }
    }
    else if (info->bpp == 32)
    {
        for (int y = 0; y < bmp->height; y++)
        {
            int src_y = top_down ? y : (bmp->height - 1 - y);

            const uint8_t *row =
                pixels + src_y * bmp->width * 4;

            for (int x = 0; x < bmp->width; x++)
            {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                uint8_t a = row[x * 4 + 3];

                if (a == 0)
                    a = 255;

                bmp->pixels[y * bmp->width + x] =
                    (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }

    return bmp;
}

void bmp_free(bitmap_t *bmp)
{
    if (!bmp)
        return;

    if (bmp->pixels)
        kfree(bmp->pixels);

    kfree(bmp);
}