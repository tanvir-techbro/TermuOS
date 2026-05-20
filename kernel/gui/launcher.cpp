#include "launcher.hpp"
#include "desktop.hpp"
#include "bitmap.hpp"
extern "C" {
#include "../drivers/video/gfx.h"
#include "../drivers/video/fb.h"
}
#include "ui.hpp"
#include <stddef.h>

// ─── Constructor ──────────────────────────────────────────────────────────────

Launcher::Launcher()
    : _apps(nullptr), _count(0)
    , _x(0), _y(0), _w(0), _h(0)
    , _visible(false), _hovered(-1)
{}

void Launcher::set_apps(App **apps, int count)
{
    _apps  = apps;
    _count = count;
}

// ─── Layout ───────────────────────────────────────────────────────────────────

int Launcher::rows() const
{
    return (_count + LAUNCHER_COLS - 1) / LAUNCHER_COLS;
}

void Launcher::item_pos(int i, int &ox, int &oy) const
{
    int col = i % LAUNCHER_COLS;
    int row = i / LAUNCHER_COLS;
    ox = _x + LAUNCHER_PAD + col * LAUNCHER_ITEM_W;
    oy = _y + LAUNCHER_TITLE_H + LAUNCHER_PAD + row * LAUNCHER_ITEM_H;
}

// ─── Show/hide ────────────────────────────────────────────────────────────────

void Launcher::show(int screen_w, int screen_h, int taskbar_y)
{
    int cols = _count < LAUNCHER_COLS ? _count : LAUNCHER_COLS;
    _w = cols * LAUNCHER_ITEM_W + LAUNCHER_PAD * 2;
    _h = LAUNCHER_TITLE_H + rows() * LAUNCHER_ITEM_H + LAUNCHER_PAD * 2;

    // Centre horizontally, sit just above taskbar
    _x = (screen_w - _w) / 2;
    _y = taskbar_y - _h - 8;
    if (_y < 0) _y = 0;

    _visible = true;
    _hovered = -1;
    draw();
}

void Launcher::hide()
{
    _visible = false;
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void Launcher::draw()
{
    if (!_visible) return;

    // Background
    gfx_fill_rect(_x, _y, _w, _h, fb_colour(18, 18, 30));
    gfx_draw_rect(_x, _y, _w, _h, fb_colour(60, 60, 110));

    // Inner top glow line
    gfx_draw_hline(_x + 1, _y + 1, _w - 2, fb_colour(80, 80, 140));

    // Title
    gfx_draw_string(_x + LAUNCHER_PAD, _y + (LAUNCHER_TITLE_H - GFX_FONT_H) / 2,
                    "All Apps", fb_colour(200, 200, 220), fb_colour(18, 18, 30));

    // Separator
    gfx_draw_hline(_x + LAUNCHER_PAD, _y + LAUNCHER_TITLE_H,
                   _w - LAUNCHER_PAD * 2, fb_colour(50, 50, 90));

    // App items
    for (int i = 0; i < _count; i++)
    {
        int ox, oy;
        item_pos(i, ox, oy);

        // Hover highlight
        if (i == _hovered)
            gfx_fill_rect(ox - 4, oy - 4,
                          LAUNCHER_ITEM_W, LAUNCHER_ITEM_H + 4,
                          fb_colour(35, 35, 60));

        // Icon
        if (_apps[i]->icon_bmp)
        {
            // Scale up 32x32 icon to 48x48 by pixel doubling
            // (simple nearest-neighbour)
            const uint8_t *bmp = _apps[i]->icon_bmp;
            // Just draw at 32x32 centred in the 48px slot
            int icon_x = ox + (LAUNCHER_ICON_SIZE - 32) / 2;
            int icon_y = oy + (LAUNCHER_ICON_SIZE - 32) / 2;
            Bitmap::draw(icon_x, icon_y, bmp, 32, 32);
        }
        else
        {
            // Coloured square fallback
            gfx_fill_rect(ox + 8, oy + 8, 32, 32, fb_colour(60, 80, 160));
            gfx_draw_rect(ox + 8, oy + 8, 32, 32, fb_colour(100, 120, 200));
            if (_apps[i]->name)
            {
                char label[3] = {
                    _apps[i]->name[0],
                    _apps[i]->name[1] ? _apps[i]->name[1] : ' ',
                    '\0'
                };
                gfx_draw_string(ox + 16, oy + 16, label,
                                fb_colour(255, 255, 255),
                                fb_colour(60, 80, 160));
            }
        }

        // App name label below icon
        if (_apps[i]->name)
        {
            // Truncate to fit
            char label[9];
            int j = 0;
            while (_apps[i]->name[j] && j < 8)
            { label[j] = _apps[i]->name[j]; j++; }
            label[j] = '\0';

            int lw = j * GFX_FONT_W;
            int lx = ox + (LAUNCHER_ITEM_W - lw) / 2;
            int ly = oy + LAUNCHER_ICON_SIZE + 2;
            gfx_draw_string(lx, ly, label,
                            fb_colour(200, 200, 220),
                            i == _hovered ? fb_colour(35, 35, 60) : fb_colour(18, 18, 30));
        }
    }
}

// ─── Input ────────────────────────────────────────────────────────────────────

bool Launcher::on_click(int mx, int my)
{
    if (!_visible) return false;

    // Click outside — dismiss
    if (mx < _x || mx >= _x + _w || my < _y || my >= _y + _h)
    {
        hide();
        return false;
    }

    // Hit test items
    for (int i = 0; i < _count; i++)
    {
        int ox, oy;
        item_pos(i, ox, oy);
        if (mx >= ox - 4 && mx < ox + LAUNCHER_ITEM_W &&
            my >= oy - 4 && my < oy + LAUNCHER_ITEM_H)
        {
            hide();
            _apps[i]->launch();
            return true;
        }
    }

    return true; // consumed
}