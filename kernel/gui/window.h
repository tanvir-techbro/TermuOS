#pragma once
#include "../drivers/video/gfx.h"
#include <stdint.h>

#define WIN_TITLE_H (GFX_FONT_H + 4) // title bar height

typedef struct 
{
    int x, y;           // screen position
    int w, h;           // total size including border and title bar

    const char *title;

    uint32_t bg;        // content area background
    uint32_t border;    // border colour
    uint32_t title_bg;  // title bar background
    uint32_t title_fg;  // title bar text colour
} window_t;

// Draw full window chrome (border, title bar, content background)
void win_draw(window_t *w);

// Draw a string inside the content area at cell offset (cx, cy)
// cx/cy are in pixels relative tot he top-left of the content area
// Clips to the window bounds
void win_draw_string(window_t *w, int cx, int cy, const char *s, uint32_t fg);

// Fill rect inside the content area (relative coords, clipped)
void win_fill_rect(window_t *w, int rx, int ry, int rw, int rh, uint32_t colour);

// Get the content area dimensions (window minus border and title bar)
int win_content_w(window_t *w);
int win_content_h(window_t *w);
