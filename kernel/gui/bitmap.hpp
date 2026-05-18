#pragma once
#include <stdint.h>

namespace Bitmap
{
    // draw a 24-bit uncompressed BPM from raw byte data at screen pos (x, y)
    void draw(int x, int y, const uint8_t *data, int w, int h);
}
