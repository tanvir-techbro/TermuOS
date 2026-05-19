#include "context_menu.hpp"
extern "C" {
#include "../drivers/video/gfx.h"
#include "../drivers/video/fb.h"
}
#include "ui.hpp"

// ─── Constructor ──────────────────────────────────────────────────────────────

ContextMenu::ContextMenu()
    : _count(0), _x(0), _y(0), _visible(false), _hovered(-1)
{
    for (int i = 0; i < CONTEXT_MENU_MAX_ITEMS; i++) {
        _items[i] = {nullptr, nullptr, nullptr, false};
    }
}

// ─── Building ─────────────────────────────────────────────────────────────────

void ContextMenu::add_item(const char *label, void (*on_click)(void *), void *userdata)
{
    if (_count >= CONTEXT_MENU_MAX_ITEMS) return;
    _items[_count++] = {label, on_click, userdata, false};
}

void ContextMenu::add_separator()
{
    if (_count >= CONTEXT_MENU_MAX_ITEMS) return;
    _items[_count++] = {nullptr, nullptr, nullptr, true};
}

// ─── Show/hide ────────────────────────────────────────────────────────────────

void ContextMenu::show(int x, int y)
{
    // Clamp to screen
    struct limine_framebuffer *fb = fb_get();
    if (fb) {
        if (x + CONTEXT_MENU_W > (int)fb->width)
            x = (int)fb->width - CONTEXT_MENU_W;
        if (y + menu_h() > (int)fb->height)
            y = (int)fb->height - menu_h();
    }
    _x = x; _y = y;
    _visible = true;
    _hovered = -1;
    draw();
}

void ContextMenu::hide()
{
    _visible = false;
}

// ─── Layout ───────────────────────────────────────────────────────────────────

int ContextMenu::menu_h() const
{
    int h = CONTEXT_MENU_PAD * 2;
    for (int i = 0; i < _count; i++)
        h += _items[i].seperator ? 5 : CONTEXT_MENU_ITEM_H;
    return h;
}

int ContextMenu::item_screen_y(int i) const
{
    int y = _y + CONTEXT_MENU_PAD;
    for (int j = 0; j < i; j++)
        y += _items[j].seperator ? 5 : CONTEXT_MENU_ITEM_H;
    return y;
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void ContextMenu::draw()
{
    if (!_visible) return;

    int w = CONTEXT_MENU_W;
    int h = menu_h();

    // Background + border
    gfx_fill_rect(_x, _y, w, h, fb_colour(28, 28, 42));
    gfx_draw_rect(_x, _y, w, h, fb_colour(80, 80, 130));

    for (int i = 0; i < _count; i++)
    {
        int iy = item_screen_y(i);

        if (_items[i].seperator)
        {
            // Separator line
            gfx_draw_hline(_x + 4, iy + 2, w - 8, fb_colour(60, 60, 100));
            continue;
        }

        // Hover highlight
        uint32_t bg = (i == _hovered)
            ? fb_colour(60, 80, 160)
            : fb_colour(28, 28, 42);

        gfx_fill_rect(_x + 1, iy, w - 2, CONTEXT_MENU_ITEM_H, bg);

        if (_items[i].label)
            gfx_draw_string(_x + CONTEXT_MENU_PAD + 4, iy + (CONTEXT_MENU_ITEM_H - GFX_FONT_H) / 2,
                            _items[i].label, fb_colour(220, 220, 220), bg);
    }
}

// ─── Input ────────────────────────────────────────────────────────────────────

bool ContextMenu::on_click(int mx, int my)
{
    if (!_visible) return false;
    if (mx < _x || mx >= _x + CONTEXT_MENU_W) return false;
    if (my < _y || my >= _y + menu_h()) return false;

    for (int i = 0; i < _count; i++)
    {
        if (_items[i].seperator) continue;

        int iy = item_screen_y(i);
        if (my >= iy && my < iy + CONTEXT_MENU_ITEM_H)
        {
            hide();
            if (_items[i].on_click)
                _items[i].on_click(_items[i].userdata);
            return true;
        }
    }
    return true; // consumed even if no item hit
}

bool ContextMenu::on_click_outside(int mx, int my)
{
    if (!_visible) return false;
    if (mx >= _x && mx < _x + CONTEXT_MENU_W &&
        my >= _y && my < _y + menu_h())
        return false; // inside — don't dismiss

    hide();
    return false;
}
