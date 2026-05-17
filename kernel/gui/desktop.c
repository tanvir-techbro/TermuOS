#include "desktop.h"
#include "bitmap.h"
#include "ui.h"
#include "../drivers/video/fb.h"
#include "../drivers/video/gfx.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/rtc/rtc.h"
#include "../arch/x86_64/pit.h"
#include "../mm/heap.h"
#include <stddef.h>

// ─── Background BMP (linked via objcopy) ─────────────────────────────────────

extern uint8_t _binary_assets_bg_stars_bmp_start[];
extern uint8_t _binary_assets_bg_stars_bmp_end[];

// ─── State ────────────────────────────────────────────────────────────────────

static uint32_t        _bg_colour;   // fallback solid colour
static int             _bg_loaded = 0;
static const app_t    *_apps[DESKTOP_MAX_APPS];
static int             _app_count = 0;
static desk_window_t   _windows[DESKTOP_MAX_WINDOWS];
static int             _focused  = -1;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static int screen_w(void) { return (int)fb_get()->width;  }
static int screen_h(void) { return (int)fb_get()->height; }
static int taskbar_y(void){ return screen_h() - DESKTOP_TASKBAR_H; }

// ─── Background ───────────────────────────────────────────────────────────────

static void draw_background(void)
{
    if (_bg_loaded)
    {
        // Draw BMP — it may not match screen resolution exactly, so fill first
        gfx_fill_rect(0, 0, screen_w(), taskbar_y(), _bg_colour);
        bmp_draw(0, 0, _binary_assets_bg_stars_bmp_start, screen_w(), taskbar_y());
    }
    else
    {
        gfx_fill_rect(0, 0, screen_w(), taskbar_y(), _bg_colour);
    }
}

// ─── Taskbar ──────────────────────────────────────────────────────────────────

static void draw_taskbar(void)
{
    int ty = taskbar_y();
    int sw = screen_w();

    gfx_fill_rect(0, ty, sw, DESKTOP_TASKBAR_H, fb_colour(22, 22, 32));
    gfx_draw_hline(0, ty, sw, fb_colour(60, 60, 100));

    // Clock on the right
    {
        rtc_time_t t;
        rtc_read(&t);

        // Format HH:MM:SS
        char clock[9];
        clock[0] = '0' + t.hour / 10;
        clock[1] = '0' + t.hour % 10;
        clock[2] = ':';
        clock[3] = '0' + t.minute / 10;
        clock[4] = '0' + t.minute % 10;
        clock[5] = ':';
        clock[6] = '0' + t.second / 10;
        clock[7] = '0' + t.second % 10;
        clock[8] = '\0';

        int cw = 8 * GFX_FONT_W; // "HH:MM:SS" = 8 chars
        int cx = sw - cw - DESKTOP_ICON_PAD;
        int cy = ty + (DESKTOP_TASKBAR_H - GFX_FONT_H) / 2;
        gfx_draw_string(cx, cy, clock, fb_colour(200, 200, 220), fb_colour(22, 22, 32));
    }

    int ix = DESKTOP_ICON_PAD;
    for (int i = 0; i < _app_count; i++)
    {
        int iy = ty + (DESKTOP_TASKBAR_H - APP_ICON_H) / 2;

        // Highlight focused app
        if (_focused >= 0 && _windows[_focused].open)
            gfx_fill_rect(ix - 2, ty + 2, APP_ICON_W + 4, DESKTOP_TASKBAR_H - 4,
                          fb_colour(40, 40, 60));

        if (_apps[i]->icon_bmp)
        {
            bmp_draw(ix, iy, _apps[i]->icon_bmp, APP_ICON_W, APP_ICON_H);
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
}

// ─── Windows ──────────────────────────────────────────────────────────────────

static void draw_windows(void)
{
    for (int i = 0; i < DESKTOP_MAX_WINDOWS; i++)
    {
        if (!_windows[i].open) continue;

        _windows[i].win.border = (i == _focused)
            ? fb_colour(80, 120, 220)
            : fb_colour(50, 50, 90);

        win_draw(&_windows[i].win);

        if (_windows[i].on_draw)
            _windows[i].on_draw(&_windows[i]);
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void desktop_init(uint32_t bg_colour)
{
    _bg_colour = bg_colour;
    _app_count = 0;
    _focused   = -1;

    // Check BMP is valid before using it
    _bg_loaded = (_binary_assets_bg_stars_bmp_start[0] == 'B' &&
                  _binary_assets_bg_stars_bmp_start[1] == 'M');

    for (int i = 0; i < DESKTOP_MAX_WINDOWS; i++)
        _windows[i].open = 0;

    ui_theme_default();
}

void desktop_add_app(const app_t *app)
{
    if (_app_count < DESKTOP_MAX_APPS)
        _apps[_app_count++] = app;
}

int desktop_open_window(const char *title, int x, int y, int w, int h,
                        void (*on_draw)(desk_window_t *dw),
                        void (*on_key)(desk_window_t *dw, char c))
{
    for (int i = 0; i < DESKTOP_MAX_WINDOWS; i++)
    {
        if (_windows[i].open) continue;

        _windows[i].open    = 1;
        _windows[i].focused = 0;
        _windows[i].on_draw = on_draw;
        _windows[i].on_key  = on_key;

        _windows[i].win.x        = x;
        _windows[i].win.y        = y;
        _windows[i].win.w        = w;
        _windows[i].win.h        = h;
        _windows[i].win.title    = title;
        _windows[i].win.bg       = ui_theme.bg;
        _windows[i].win.border   = ui_theme.border;
        _windows[i].win.title_bg = ui_theme.accent;
        _windows[i].win.title_fg = fb_colour(255, 255, 255);

        _focused = i;
        return i;
    }
    return -1;
}

void desktop_close_window(int handle)
{
    if (handle < 0 || handle >= DESKTOP_MAX_WINDOWS) return;
    _windows[handle].open = 0;
    if (_focused == handle) _focused = -1;
    desktop_redraw();
}

desk_window_t *desktop_get_window(int handle)
{
    if (handle < 0 || handle >= DESKTOP_MAX_WINDOWS) return (void*)0;
    return &_windows[handle];
}

void desktop_redraw(void)
{
    draw_background();
    draw_windows();
    draw_taskbar();
}

void desktop_run(void)
{
    desktop_redraw();

    uint32_t last_second = 0;

    for (;;)
    {
        uint32_t now = (uint32_t)(pit_ticks() / 100);
        if (now != last_second)
        {
            last_second = now;
            draw_taskbar();
        }

        if (!keyboard_haschar()) continue;
        char c = keyboard_getchar();

        if (_focused >= 0 && _windows[_focused].open)
            if (_windows[_focused].on_key)
                _windows[_focused].on_key(&_windows[_focused], c);
    }
}

