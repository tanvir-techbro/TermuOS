#include "window.h"
#include "../drivers/video/gfx.h"

// Content area helpers

// Content area starts 1px in for border, then WIN_TITLE_H for the title bar
#define BORDER_W 1

static int content_x(window_t *w) { return w->x + BORDER_W; }
static int content_y(window_t *w) { return w->y + BORDER_W + WIN_TITLE_H; }

int win_content_w(window_t *w) { return w->w - BORDER_W * 2; }
int win_content_h(window_t *w) { return w->h - BORDER_W * 2 - WIN_TITLE_H; }

// Draw
void win_draw(window_t *w)
{
    // Outer border
    gfx_draw_rect(w->x, w->y, w->w, w->h, w->border);

    // Title bar background
    gfx_fill_rect(w->x + BORDER_W,
                  w->y + BORDER_W,
                  w->w - BORDER_W * 2,
                  WIN_TITLE_H,
                  w->title_bg);

    // Title text (2px padding inside title bar)
    if (w->title)
        gfx_draw_string(w->x + BORDER_W + 4,
                        w->y + BORDER_W + 2,
                        w->title,
                        w->title_fg,
                        w->title_bg);

    // Separator line between title bar and content
    gfx_draw_hline(w->x + BORDER_W,
                   w->y + BORDER_W + WIN_TITLE_H,
                   w->w - BORDER_W * 2,
                   w->border);

    // Content area background
    gfx_fill_rect(content_x(w),
                  content_y(w),
                  win_content_w(w),
                  win_content_h(w),
                  w->bg);
}

// Content drawing
void win_draw_string(window_t *w, int cx, int cy, const char *s, uint32_t fg)
{
    if (!s) return;

    int x0 = content_x(w) + cx;
    int y0 = content_y(w) + cy;
    int x_start = x0;

    int max_x = content_x(w) + win_content_w(w);
    int max_y = content_y(w) + win_content_h(w);

    // gfx_draw_char(x0, y0, s[0], fg, w->bg);
    // gfx_draw_char(x0 + GFX_FONT_W, y0, s[1], fg, w->bg);

    while (*s)
    {
        if (y0 + GFX_FONT_H > max_y) break;

        if (*s == '\n')
        {
            x0  = x_start;
            y0 += GFX_FONT_H;
        }
        else
        {
            if (x0 + GFX_FONT_W <= max_x)
                gfx_draw_char(x0, y0, *s, fg, w->bg);
            x0 += GFX_FONT_W;
            if (x0 >= max_x)
            {
                x0  = x_start;
                y0 += GFX_FONT_H;
            }
        }
        s++;
    }
}

void win_fill_rect(window_t *w, int rx, int ry, int rw, int rh, uint32_t colour)
{
    int cx = content_x(w);
    int cy = content_y(w);
    int cw = win_content_w(w);
    int ch = win_content_h(w);

    // Clip to content area
    int x0 = cx + rx;
    int y0 = cy + ry;
    int x1 = x0 + rw;
    int y1 = y0 + rh;

    if (x0 < cx) x0 = cx;
    if (y0 < cy) y0 = cy;
    if (x1 > cx + cw) x1 = cx + cw;
    if (y1 > cy + ch) y1 = cy + ch;

    if (x1 <= x0 || y1 <= y0) return;

    gfx_fill_rect(x0, y0, x1 - x0, y1 - y0, colour);
}
