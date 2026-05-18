#include "desktop.hpp"
#include "bitmap.hpp"
#include "ui.hpp"
extern "C" {
#include "../drivers/video/fb.h"
#include "../drivers/video/gfx.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/input/mouse.h"
#include "../drivers/rtc/rtc.h"
#include "../arch/x86_64/pit.h"
}

extern uint8_t _binary_assets_bg_stars_bmp_start[];

// ─── Cursor ───────────────────────────────────────────────────────────────────

#define CURSOR_W 11
#define CURSOR_H 18

// Simple arrow cursor — 1=fg, 0=transparent, 2=outline
static const uint8_t cursor_shape[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,1,1,1,1,1},
    {1,2,2,1,2,2,1,0,0,0,0},
    {1,2,1,0,1,2,2,1,0,0,0},
    {1,1,0,0,1,2,2,1,0,0,0},
    {1,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,1,1,0,0,0},
};

static void draw_cursor(int x, int y)
{
    uint32_t fg      = fb_colour(255, 255, 255);
    uint32_t outline = fb_colour(0,   0,   0);

    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
        {
            uint8_t v = cursor_shape[row][col];
            if (v == 1) fb_putpixel(x + col, y + row, fg);
            else if (v == 2) fb_putpixel(x + col, y + row, outline);
        }
}

// Save/restore pixels under cursor to avoid permanent overdraw
static uint32_t _cursor_save[CURSOR_H * CURSOR_W];
static int _cursor_saved_x = -1, _cursor_saved_y = -1;

static void cursor_save(int x, int y)
{
    struct limine_framebuffer *fbp = fb_get();
    if (!fbp) return;
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
        {
            int px = x + col, py = y + row;
            if (px >= 0 && py >= 0 && (uint64_t)px < fbp->width && (uint64_t)py < fbp->height)
            {
                uint32_t *p = (uint32_t *)((uint8_t *)fbp->address + py * fbp->pitch);
                _cursor_save[row * CURSOR_W + col] = p[px];
            }
            else _cursor_save[row * CURSOR_W + col] = 0;
        }
    _cursor_saved_x = x;
    _cursor_saved_y = y;
}

static void cursor_restore(void)
{
    if (_cursor_saved_x < 0) return;
    struct limine_framebuffer *fbp = fb_get();
    if (!fbp) return;
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
        {
            int px = _cursor_saved_x + col, py = _cursor_saved_y + row;
            if (px >= 0 && py >= 0 && (uint64_t)px < fbp->width && (uint64_t)py < fbp->height)
            {
                uint32_t *p = (uint32_t *)((uint8_t *)fbp->address + py * fbp->pitch);
                p[px] = _cursor_save[row * CURSOR_W + col];
            }
        }
    _cursor_saved_x = -1;
}

// ─── Singleton ────────────────────────────────────────────────────────────────

Desktop &Desktop::instance()
{
    static Desktop _instance;
    return _instance;
}

// ─── Screen helpers ───────────────────────────────────────────────────────────

int Desktop::screen_w() const { return (int)fb_get()->width;  }
int Desktop::screen_h() const { return (int)fb_get()->height; }

// ─── Init ─────────────────────────────────────────────────────────────────────

void Desktop::init(uint32_t bg_colour)
{
    _bg_colour = bg_colour;
    _app_count = 0;
    _focused   = -1;

    _bg_loaded = (_binary_assets_bg_stars_bmp_start[0] == 'B' &&
                  _binary_assets_bg_stars_bmp_start[1] == 'M');

    for (int i = 0; i < DESKTOP_MAX_WINDOWS; i++) _windows[i] = nullptr;
    for (int i = 0; i < DESKTOP_MAX_APPS;    i++) _apps[i]    = nullptr;

    Theme::set_default();
}

// ─── Apps ─────────────────────────────────────────────────────────────────────

void Desktop::add_app(App *app)
{
    if (_app_count < DESKTOP_MAX_APPS)
        _apps[_app_count++] = app;
}

// ─── Windows ──────────────────────────────────────────────────────────────────

int Desktop::open_window(Window *win)
{
    for (int i = 0; i < DESKTOP_MAX_WINDOWS; i++)
    {
        if (_windows[i]) continue;
        _windows[i] = win;
        _focused = i;
        return i;
    }
    return -1;
}

void Desktop::close_window(int handle)
{
    if (handle < 0 || handle >= DESKTOP_MAX_WINDOWS) return;
    _windows[handle] = nullptr;
    if (_focused == handle) _focused = -1;
    redraw();
}

Window *Desktop::get_window(int handle)
{
    if (handle < 0 || handle >= DESKTOP_MAX_WINDOWS) return nullptr;
    return _windows[handle];
}

// ─── Drawing ──────────────────────────────────────────────────────────────────

void Desktop::draw_background()
{
    gfx_fill_rect(0, 0, screen_w(), taskbar_y(), _bg_colour);
    if (_bg_loaded)
        Bitmap::draw(0, 0, _binary_assets_bg_stars_bmp_start, screen_w(), taskbar_y());
}

void Desktop::draw_taskbar()
{
    int ty = taskbar_y();
    int sw = screen_w();

    gfx_fill_rect(0, ty, sw, DESKTOP_TASKBAR_H, fb_colour(22, 22, 32));
    gfx_draw_hline(0, ty, sw, fb_colour(60, 60, 100));

    int ix = DESKTOP_ICON_PAD;
    for (int i = 0; i < _app_count; i++)
    {
        int iy = ty + (DESKTOP_TASKBAR_H - APP_ICON_H) / 2;

        if (_apps[i]->icon_bmp)
        {
            Bitmap::draw(ix, iy, _apps[i]->icon_bmp, APP_ICON_W, APP_ICON_H);
        }
        else
        {
            gfx_fill_rect(ix, iy, APP_ICON_W, APP_ICON_H, fb_colour(60, 80, 160));
            gfx_draw_rect(ix, iy, APP_ICON_W, APP_ICON_H, fb_colour(100, 120, 200));
            if (_apps[i]->name)
            {
                char label[3] = {
                    _apps[i]->name[0],
                    _apps[i]->name[1] ? _apps[i]->name[1] : ' ',
                    '\0'
                };
                gfx_draw_string(ix + 8, iy + 8, label,
                                fb_colour(255, 255, 255),
                                fb_colour(60, 80, 160));
            }
        }
        ix += APP_ICON_W + DESKTOP_ICON_PAD;
    }

    // Clock
    {
        rtc_time_t t;
        rtc_read(&t);
        char clock[9];
        uint8_t hour = (t.hour + 1) % 24;
        clock[0] = '0' + hour / 10;
        clock[1] = '0' + hour % 10;
        clock[2] = ':';
        clock[3] = '0' + t.minute / 10;
        clock[4] = '0' + t.minute % 10;
        clock[5] = ':';
        clock[6] = '0' + t.second / 10;
        clock[7] = '0' + t.second % 10;
        clock[8] = '\0';
        int cw = 8 * GFX_FONT_W;
        int cx = sw - cw - DESKTOP_ICON_PAD;
        int cy = ty + (DESKTOP_TASKBAR_H - GFX_FONT_H) / 2;
        gfx_draw_string(cx, cy, clock, fb_colour(200, 200, 220), fb_colour(22, 22, 32));
    }
}

void Desktop::draw_windows()
{
    for (int i = 0; i < DESKTOP_MAX_WINDOWS; i++)
    {
        if (!_windows[i]) continue;
        _windows[i]->border = (i == _focused)
            ? fb_colour(80, 120, 220)
            : fb_colour(50, 50, 90);
        _windows[i]->draw();
    }
}

void Desktop::redraw()
{
    draw_background();
    draw_windows();
    draw_taskbar();
}

// ─── Click handling ───────────────────────────────────────────────────────────

void Desktop::handle_click(int mx, int my)
{
    // Check taskbar icons
    if (my >= taskbar_y())
    {
        int ix = DESKTOP_ICON_PAD;
        for (int i = 0; i < _app_count; i++)
        {
            int iy = taskbar_y() + (DESKTOP_TASKBAR_H - APP_ICON_H) / 2;
            if (mx >= ix && mx < ix + APP_ICON_W &&
                my >= iy && my < iy + APP_ICON_H)
            {
                _apps[i]->launch();
                return;
            }
            ix += APP_ICON_W + DESKTOP_ICON_PAD;
        }
        return;
    }

    // Check windows — top to bottom (reverse order = topmost first)
    for (int i = DESKTOP_MAX_WINDOWS - 1; i >= 0; i--)
    {
        if (!_windows[i]) continue;
        Window *w = _windows[i];

        // Hit test window bounds
        if (mx >= w->x && mx < w->x + w->w &&
            my >= w->y && my < w->y + w->h)
        {
            // Focus this window
            if (_focused != i)
            {
                _focused = i;
                redraw();
            }

            // Route click to window
            w->on_click(mx, my);
            return;
        }
    }
}

// ─── Run loop ─────────────────────────────────────────────────────────────────

void Desktop::run()
{
    redraw();

    uint32_t last_second = 0;
    int last_mx = -1, last_my = -1;
    int last_left = 0;

    for (;;)
    {
        // Clock tick
        uint32_t now = (uint32_t)(pit_ticks() / 100);
        if (now != last_second)
        {
            last_second = now;
            cursor_restore();
            draw_taskbar();
            mouse_state_t ms = mouse_get();
            cursor_save(ms.x, ms.y);
            draw_cursor(ms.x, ms.y);
        }

        // Mouse
        if (mouse_moved())
        {
            mouse_state_t ms = mouse_get();

            cursor_restore();
            cursor_save(ms.x, ms.y);
            draw_cursor(ms.x, ms.y);

            // Left click (rising edge)
            if (ms.left && !last_left)
                handle_click(ms.x, ms.y);

            last_left = ms.left;
            last_mx = ms.x;
            last_my = ms.y;
        }

        // Keyboard
        if (keyboard_haschar())
        {
            char c = keyboard_getchar();
            if (_focused >= 0 && _windows[_focused])
                _windows[_focused]->on_key(c);

            // redraw cursor after window redraws content
            mouse_state_t ms = mouse_get();
            cursor_save(ms.x, ms.y);
            draw_cursor(ms.x, ms.y);
        }
    }
}
