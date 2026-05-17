#pragma once
#include <stdint.h>

// Draws a 24-bit uncompressed BMP from raw byte data at screen position (x, y).
// The BMP is bottom-up by default (standard BMP format).
// w and h are the expected dimensions — used for bounds checking only.
void bmp_draw(int x, int y, const uint8_t *data, int w, int h);