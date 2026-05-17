#include "ui.h"
#include "window.h"
#include "../drivers/video/gfx.h"
#include "../drivers/video/fb.h"
#include "../lib/string.h"

// ─── Theme ────────────────────────────────────────────────────────────────────

ui_theme_t ui_theme;

void ui_theme_default(void)
{
    ui_theme.fg      = fb_colour(220, 220, 220);
    ui_theme.bg      = fb_colour(30,  30,  40);
    ui_theme.border  = fb_colour(80,  80,  120);
    ui_theme.accent  = fb_colour(60,  100, 200);
    ui_theme.hover   = fb_colour(80,  120, 220);
    ui_theme.pressed = fb_colour(40,  70,  160);
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Convert content-relative coords to screen coords
static int scr_x(window_t *win, int rx)
{
    return win->x + 1 + rx;  // 1px border
}

static int scr_y(window_t *win, int ry)
{
    return win->y + 1 + WIN_TITLE_H + ry;
}

// ─── Label ────────────────────────────────────────────────────────────────────

void ui_draw_label(window_t *win, ui_label_t *l)
{
    win_draw_string(win, l->x, l->y, l->text, l->fg ? l->fg : ui_theme.fg);
}

// ─── Button ───────────────────────────────────────────────────────────────────

void ui_draw_button(window_t *win, ui_button_t *b)
{
    int sx = scr_x(win, b->x);
    int sy = scr_y(win, b->y);

    uint32_t col = b->pressed ? ui_theme.pressed : ui_theme.accent;

    gfx_fill_rect(sx, sy, b->w, b->h, col);
    gfx_draw_rect(sx, sy, b->w, b->h, ui_theme.border);

    // Centre text inside button
    if (b->text)
    {
        int tw = 0;
        for (const char *p = b->text; *p; p++) tw += GFX_FONT_W;
        int tx = sx + (b->w - tw) / 2;
        int ty = sy + (b->h - GFX_FONT_H) / 2;
        gfx_draw_string(tx, ty, b->text, ui_theme.fg, col);
    }
}

int ui_button_hit(window_t *win, ui_button_t *b, int mx, int my)
{
    int sx = scr_x(win, b->x);
    int sy = scr_y(win, b->y);
    return mx >= sx && mx < sx + b->w &&
           my >= sy && my < sy + b->h;
}

// ─── Textbox ──────────────────────────────────────────────────────────────────

void ui_draw_textbox(window_t *win, ui_textbox_t *t)
{
    int sx = scr_x(win, t->x);
    int sy = scr_y(win, t->y);
    int h  = GFX_FONT_H + 4;

    uint32_t bg = t->focused ? fb_colour(20, 20, 35) : fb_colour(15, 15, 25);

    gfx_fill_rect(sx, sy, t->w, h, bg);
    gfx_draw_rect(sx, sy, t->w, h,
                  t->focused ? ui_theme.accent : ui_theme.border);

    // Draw text with 2px padding
    gfx_draw_string(sx + 2, sy + 2, t->buf, ui_theme.fg, bg);

    // Cursor
    if (t->focused)
    {
        int cx = sx + 2 + t->len * GFX_FONT_W;
        gfx_draw_vline(cx, sy + 2, GFX_FONT_H, ui_theme.fg);
    }
}

void ui_textbox_input(ui_textbox_t *t, char c)
{
    if (c == '\b')
    {
        if (t->len > 0) t->buf[--t->len] = '\0';
        return;
    }
    if (c == '\n' || c == '\r') return;
    if (t->len < UI_TEXTBOX_MAX - 1)
    {
        t->buf[t->len++] = c;
        t->buf[t->len]   = '\0';
    }
}

// ─── List ─────────────────────────────────────────────────────────────────────

void ui_draw_list(window_t *win, ui_list_t *l)
{
    int sx = scr_x(win, l->x);
    int sy = scr_y(win, l->y);

    gfx_fill_rect(sx, sy, l->w, l->h, ui_theme.bg);
    gfx_draw_rect(sx, sy, l->w, l->h, ui_theme.border);

    int visible = l->h / GFX_FONT_H;
    int end     = l->scroll + visible;
    if (end > l->count) end = l->count;

    for (int i = l->scroll; i < end; i++)
    {
        int iy = sy + (i - l->scroll) * GFX_FONT_H;
        uint32_t bg = (i == l->selected) ? ui_theme.accent : ui_theme.bg;
        gfx_fill_rect(sx + 1, iy, l->w - 2, GFX_FONT_H, bg);
        if (l->items[i])
            gfx_draw_string(sx + 4, iy, l->items[i], ui_theme.fg, bg);
    }
}

int ui_list_hit(window_t *win, ui_list_t *l, int mx, int my)
{
    int sx = scr_x(win, l->x);
    int sy = scr_y(win, l->y);

    if (mx < sx || mx >= sx + l->w) return 0;
    if (my < sy || my >= sy + l->h) return 0;

    int idx = l->scroll + (my - sy) / GFX_FONT_H;
    if (idx >= 0 && idx < l->count)
    {
        l->selected = idx;
        return 1;
    }
    return 0;
}