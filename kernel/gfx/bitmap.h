#pragma once
#include <stdint.h>

typedef struct
{
    int width;
    int height;
    uint32_t *pixels;
} bitmap_t;

typedef struct __attribute__((packed))
{
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} bmp_file_header_t;

typedef struct __attribute__((packed))
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t xppm;
    int32_t yppm;
    uint32_t colors_used;
    uint32_t important_colors;
} bmp_info_header_t;

bitmap_t *bmp_load(const uint8_t *data);

void bmp_free(bitmap_t *bmp);
