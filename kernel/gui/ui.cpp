#include "ui.hpp"
#include "window.hpp"
extern "C" {
#include "../drivers/video/gfx.h"
#include "../drivers/video/fb.h"
}

// Theme

static Theme _theme;

Theme &Theme::get() { return _theme; }

void Theme::set_default()
{
    _theme.fg = fb_colour(220, 220, 220);
    _theme.bg = fb_colour(30, 30, 40);
    _theme.border = fb_colour(80, 80, 120);
    _theme.accent = fb_colour(60, 100, 200);
    _theme.hover = fb_colour(80, 120, 220);
    _theme.pressed = fb_colour(40, 70, 160);
}

// Helpers

static int scr_x(Window *win, int rx) { return win->content_x() + rx; }
static int scr_y(Window *win, int ry) { return win->content_y() + ry; }

// Label

Label::Label(int x, int y, const char *text, uint32_t fg)
    : Widget(x, y, 0, GFX_FONT_H), text(text), fg(fg ? fg : Theme::get().fg)
{}

void Label::draw(Window *win)
{
    if (!text) return;
    gfx_draw_string(scr_x(win, x), scr_y(win, y), text, fg, win->bg);
}

// Button

Button::Button(int x, int y, int w, int h, const char *text, void (*on_press)(Button *))
    : Widget(x, y, w, h), text(text), pressed(false), on_press(on_press)
    , _sx(0), _sy(0)
{}

void Button::draw(Window *win)
{
    _sx = scr_x(win, x);
    _sy = scr_y(win, y);

    uint32_t col = pressed ? Theme::get().pressed : Theme::get().accent;
    gfx_fill_rect(_sx, _sy, w, h, col);
    gfx_draw_rect(_sx, _sy, w, h, Theme::get().border);

    if (text)
    {
        int tw = 0;
        for (const char *p = text; *p; p++) tw += GFX_FONT_W;
        int tx = _sx + (w - tw) / 2;
        int ty = _sy + (h - GFX_FONT_H) / 2;
        gfx_draw_string(tx, ty, text, Theme::get().fg, col);
    }
}

bool Button::on_click(int mx, int my)
{
    if (mx >= _sx && mx < _sx + w && my >= _sy && my < _sy + h)
    {
        pressed = true;
        if (on_press) on_press(this);
        return true;
    }
    return false;
}

// Textbox

Textbox::Textbox(int x, int y, int w, void (*on_submit)(Textbox *))
    : Widget(x, y, w, GFX_FONT_H + 4)
    , len(0), focused(false), on_submit(on_submit)
    , _sx(0), _sy(0)
{
    for (int i = 0; i < TEXTBOX_MAX; i++) buf[i] = '\0';
}

void Textbox::clear()
{
    for (int i = 0; i < TEXTBOX_MAX; i++) buf[i] = '\0';
    len = 0;
}

void Textbox::draw(Window *win)
{
    _sx = scr_x(win, x);
    _sy = scr_y(win, y);

    uint32_t bg = focused ? fb_colour(20, 20, 35) : fb_colour(15, 15, 25);
    gfx_fill_rect(_sx, _sy, w, h, bg);
    gfx_draw_rect(_sx, _sy, w, h, focused ? Theme::get().accent : Theme::get().border);
    gfx_draw_string(_sx + 2, _sy + 2, buf, Theme::get().fg, bg);

    if (focused)
        gfx_draw_vline(_sx + 2 + len * GFX_FONT_W, _sy + 2, GFX_FONT_H, Theme::get().fg);
}

void Textbox::on_key(char c)
{
    if (c == '\b')
    {
        if (len > 0) buf[--len] = '\0';
        return;
    }
    if (c == '\n' || c == '\r')
    {
        if (on_submit) on_submit(this);
        return;
    }
    if (len < TEXTBOX_MAX - 1)
    {
        buf[len++] = c;
        buf[len] = '\0';
    }
}

bool Textbox::on_click(int mx, int my)
{
    if (mx >= _sx && mx < _sx + w && my >= _sy && my < _sy + h)
    {
        focused = true;
        return true;
    }
    focused = false;
    return false;
}

// List

List::List(int x, int y, int w, int h, void (*on_select)(List *, int))
    : Widget(x, y, w, h)
    , count(0), selected(-1), scroll(0), on_select(on_select)
    , _sx(0), _sy(0)
{
    for (int i = 0; i < LIST_MAX_ITEMS; i++) items[i] = nullptr;
}

void List::add_item(const char *item)
{
    if (count < LIST_MAX_ITEMS) items[count++] = item;
}

void List::clear_items()
{
    count = 0; selected = -1; scroll = 0;
    for (int i = 0; i < LIST_MAX_ITEMS; i++) items[i] = nullptr;
}

void List::draw(Window *win)
{
    _sx = scr_x(win, x);
    _sy = scr_y(win, y);

    gfx_fill_rect(_sx, _sy, w, h, Theme::get().bg);
    gfx_draw_rect(_sx, _sy, w, h, Theme::get().border);

    int visible = h / GFX_FONT_H;
    int end = scroll + visible;
    if (end > count) end = count;

    for (int i = scroll; i < end; i++)
    {
        int iy = _sy + (i - scroll) * GFX_FONT_H;
        uint32_t bg = (i == selected) ? Theme::get().accent : Theme::get().bg;
        gfx_fill_rect(_sx + 1, iy, w - 2, GFX_FONT_H, bg);
        if (items[i])
            gfx_draw_string(_sx + 4, iy, items[i], Theme::get().fg, bg);
    }
}

bool List::on_click(int mx, int my)
{
    if (mx < _sx || mx >= _sx + w) return false;
    if (my < _sy || my >= _sy + h) return false;

    int idx = scroll + (my - _sy) / GFX_FONT_H;
    if (idx >= 0 && idx < count)
    {
        selected = idx;
        if (on_select) on_select(this, idx);
        return true;
    }
    return false;
}
