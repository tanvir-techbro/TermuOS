#include "desktop.h"
#include "../gfx/gfx.h"
#include "../gfx/bitmap.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/input/mouse.h"
#include "../arch/x86_64/pit.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

extern uint8_t _binary_assets_download_bmp_start[];

static bitmap_t *terminal_icon;

// ─── Palette ──────────────────────────────────────────────────────────────────
#define COL_BG_TOP RGB(0x1a, 0x1a, 0x2e)
#define COL_BG_BOT RGB(0x0f, 0x3d, 0x4a)
#define COL_ACCENT RGB(0x48, 0xda, 0xff)
#define COL_PANEL_BG RGB(0x1c, 0x1c, 0x1e)
#define COL_PANEL_TXT RGB(0xff, 0xff, 0xff)
#define COL_PANEL_DIM RGB(0x8e, 0x8e, 0x93)
#define COL_DOCK_BG RGB(0x2c, 0x2c, 0x2e)
#define COL_DOCK_BORDER RGB(0x3a, 0x3a, 0x3c)
#define COL_WIN_BG RGB(0x1c, 0x1c, 0x1e)
#define COL_WIN_TITLE RGB(0x2c, 0x2c, 0x2e)
#define COL_WIN_TXT RGB(0xff, 0xff, 0xff)
#define COL_RED_BTN RGB(0xff, 0x5f, 0x57)
#define COL_YEL_BTN RGB(0xff, 0xbd, 0x2e)
#define COL_GRN_BTN RGB(0x28, 0xc8, 0x41)
#define COL_ICON_SHD RGB(0x00, 0x00, 0x00)

static int W, H;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static int str_len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static void text_centre(int x, int y, int w, const char *s, uint32_t fg, uint32_t bg)
{
    int tw = str_len(s) * 8;
    gfx_text(x + (w - tw) / 2, y, s, fg, bg);
}

static void text_shadow(int x, int y, const char *s, uint32_t fg)
{
    gfx_text(x + 1, y + 1, s, COL_ICON_SHD, 0);
    gfx_text(x, y, s, fg, 0);
}

// ─── Wallpaper ────────────────────────────────────────────────────────────────

static void draw_wallpaper(void)
{
    for (int i = 0; i < H - 32 - 72; i++)
    {
        int t = i * 256 / (H - 32 - 72);
        uint8_t r = (uint8_t)(0x1a + (int)(0x0f - 0x1a) * t / 256);
        uint8_t g = (uint8_t)(0x1a + (int)(0x3d - 0x1a) * t / 256);
        uint8_t b = (uint8_t)(0x2e + (int)(0x4a - 0x2e) * t / 256);
        gfx_hline(0, 32 + i, W, gfx_rgb(r, g, b));
    }

    // Accent glows
    for (int r = 120; r > 0; r -= 6)
    {
        uint8_t a = (uint8_t)(r / 6);
        gfx_circle(W / 4, H * 3 / 4, r, gfx_rgb(0, a, a * 2));
    }
    for (int r = 100; r > 0; r -= 6)
    {
        uint8_t a = (uint8_t)(r / 5);
        gfx_circle(W * 3 / 4, H / 3, r, gfx_rgb(a, a * 2, 0x4a));
    }

    // Watermark
    int tx = W / 2 - 4 * 8;
    gfx_text(tx + 1, H / 2 - 19, "TermuOS", gfx_rgb(0, 0, 0), 0);
    gfx_text(tx, H / 2 - 20, "TermuOS", RGB(0x48, 0xda, 0xff), 0);
    text_centre(0, H / 2 - 6, W, "0.1.0", RGB(0x48, 0x8a, 0xa0), 0);
}

// ─── Menubar ──────────────────────────────────────────────────────────────────

static char clock_str[8] = "00:00";

static void update_clock(void)
{
    uint64_t t = pit_ticks(), s = t / 100, m = (s / 60) % 60, h = (s / 3600) % 24;
    clock_str[0] = '0' + h / 10;
    clock_str[1] = '0' + h % 10;
    clock_str[2] = ':';
    clock_str[3] = '0' + m / 10;
    clock_str[4] = '0' + m % 10;
    clock_str[5] = 0;
}

static void draw_menubar(void)
{
    gfx_fill_rect(0, 0, W, 32, COL_PANEL_BG);
    gfx_hline(0, 31, W, RGB(0x3a, 0x3a, 0x3c));

    // Logo
    gfx_fill_circle(20, 16, 10, RGB(0x48, 0xda, 0xff));
    gfx_text(16, 12, "T", COL_PANEL_BG, 0);

    // Menus
    const char *menus[] = {"File", "Edit", "View", "Terminal", "Help", NULL};
    int mx = 46;
    for (int i = 0; menus[i]; i++)
    {
        gfx_text(mx, 12, menus[i], COL_PANEL_TXT, COL_PANEL_BG);
        mx += str_len(menus[i]) * 8 + 16;
    }

    // Clock
    update_clock();
    int cw = str_len(clock_str) * 8;
    gfx_text(W - cw - 12, 12, clock_str, COL_PANEL_TXT, COL_PANEL_BG);
    gfx_vline(W - cw - 20, 6, 20, RGB(0x3a, 0x3a, 0x3c));

    // WiFi arcs
    int wx = W - cw - 50;
    gfx_fill_rect(wx - 12, 6, 24, 26, COL_PANEL_BG);
    gfx_fill_circle(wx, 22, 2, COL_PANEL_TXT);
    gfx_circle(wx, 22, 5, COL_PANEL_TXT);
    gfx_circle(wx, 22, 9, COL_PANEL_TXT);
    gfx_fill_rect(wx - 12, 6, 24, 16, COL_PANEL_BG);

    // Battery
    int bx = W - cw - 80;
    gfx_rect(bx - 10, 8, 18, 10, COL_PANEL_TXT);
    gfx_fill_rect(bx + 8, 11, 3, 4, COL_PANEL_TXT);
    gfx_fill_rect(bx - 9, 9, 14, 8, RGB(0x28, 0xc8, 0x41));
}

// ─── Dock ─────────────────────────────────────────────────────────────────────

#define N_APPS 6

typedef struct
{
    const char *label;
    uint32_t bg;
    uint32_t fg;
    const char *sym;
} dock_app_t;

static dock_app_t dock_apps[N_APPS];

static void init_dock_apps(void)
{
    dock_apps[0] = (dock_app_t){"Terminal", RGB(0x1e, 0x1e, 0x1e), RGB(0x48, 0xda, 0xff), ">_"};
    dock_apps[1] = (dock_app_t){"Files", RGB(0x30, 0x6e, 0xff), RGB(0xff, 0xff, 0xff), "[]"};
    dock_apps[2] = (dock_app_t){"Network", RGB(0x20, 0x8a, 0x6a), RGB(0xff, 0xff, 0xff), "><"};
    dock_apps[3] = (dock_app_t){"Settings", RGB(0x6e, 0x6e, 0x7e), RGB(0xff, 0xff, 0xff), "::"};
    dock_apps[4] = (dock_app_t){"Browser", RGB(0xff, 0x5f, 0x57), RGB(0xff, 0xff, 0xff), "W "};
    dock_apps[5] = (dock_app_t){"Music", RGB(0x9b, 0x59, 0xb6), RGB(0xff, 0xff, 0xff), ">>"};
}

// Returns dock icon index under (mx,my) or -1
static int dock_hit_test(int mx, int my)
{
    int icon_w = 64, padding = 12;
    int dock_w = N_APPS * icon_w + padding * 2;
    int dock_x = (W - dock_w) / 2;
    int dock_y = H - 72;

    if (my < dock_y || my >= H)
        return -1;

    for (int i = 0; i < N_APPS; i++)
    {
        int cx = dock_x + padding + i * icon_w + icon_w / 2;
        int ix = cx - 26, iy = dock_y - 52;
        if (mx >= ix && mx < ix + 52 && my >= iy && my < iy + 52)
            return i;
    }
    return -1;
}

static void draw_dock_icon(int cx, int by, const dock_app_t *app, int hover)
{
    int sz = hover ? 58 : 52;
    int x = cx - sz / 2, y = by - sz;

    gfx_fill_rounded_rect(x + 3, y + 3, sz, sz, 12, RGB(0, 0, 0));
    gfx_fill_rounded_rect(x, y, sz, sz, 12, app->bg);
    gfx_rounded_rect(x, y, sz, sz, 12, RGB(0x5a, 0x5a, 0x5a));

    int sw = str_len(app->sym) * 8;
    gfx_text(x + (sz - sw) / 2, y + sz / 2 - 4, app->sym, app->fg, app->bg);

    if (hover)
    {
        int lw = str_len(app->label) * 8;
        gfx_fill_rounded_rect(cx - lw / 2 - 6, by + 6, lw + 12, 18, 6, COL_PANEL_BG);
        gfx_rounded_rect(cx - lw / 2 - 6, by + 6, lw + 12, 18, 6, COL_DOCK_BORDER);
        gfx_text(cx - lw / 2, by + 10, app->label, COL_PANEL_TXT, COL_PANEL_BG);
    }

    gfx_fill_circle(cx, by + 6, 3, RGB(0x48, 0xda, 0xff));
}

static void draw_dock(int hovered)
{
    int icon_w = 64, padding = 12;
    int dock_w = N_APPS * icon_w + padding * 2;
    int dock_x = (W - dock_w) / 2;
    int dock_y = H - 72;

    gfx_fill_rounded_rect(dock_x, dock_y + 8, dock_w, 64, 16, COL_DOCK_BG);
    gfx_rounded_rect(dock_x, dock_y + 8, dock_w, 64, 16, COL_DOCK_BORDER);
    gfx_hline(dock_x + 16, dock_y + 9, dock_w - 32, RGB(0x5a, 0x5a, 0x5e));

    for (int i = 0; i < N_APPS; i++)
    {
        int cx = dock_x + padding + i * icon_w + icon_w / 2;
        draw_dock_icon(cx, dock_y + 4, &dock_apps[i], i == hovered);
    }
}

// ─── Desktop icons ────────────────────────────────────────────────────────────

static void draw_desktop_icon(int x, int y, const char *label,
                              uint32_t bg, const char *sym)
{
    gfx_fill_rounded_rect(x, y, 60, 60, 14, bg);
    gfx_rounded_rect(x, y, 60, 60, 14, RGB(0x5a, 0x5a, 0x5a));
    int sw = str_len(sym) * 8;
    gfx_text(x + (60 - sw) / 2, y + 22, sym, GFX_WHITE, bg);
    int lw = str_len(label) * 8;
    text_shadow(x + (60 - lw) / 2, y + 66, label, GFX_WHITE);
}

static void draw_desktop_icons(void)
{
    draw_desktop_icon(24, 48, "Home", RGB(0x30, 0x6e, 0xff), "~");
    draw_desktop_icon(24, 138, "Docs", RGB(0x20, 0x8a, 0xd0), "D");
    draw_desktop_icon(24, 228, "Trash", RGB(0x6e, 0x6e, 0x7e), "X");
}

// ─── Window ───────────────────────────────────────────────────────────────────

static void draw_window(int x, int y, int w, int h, const char *title)
{
    gfx_fill_rounded_rect(x + 4, y + 6, w, h, 14, RGB(0, 0, 0));
    gfx_fill_rounded_rect(x, y, w, h, 14, COL_WIN_BG);
    gfx_rounded_rect(x, y, w, h, 14, RGB(0x3a, 0x3a, 0x3c));
    gfx_fill_rounded_rect(x, y, w, 32, 14, COL_WIN_TITLE);
    gfx_fill_rect(x, y + 14, w, 18, COL_WIN_TITLE);
    gfx_fill_circle(x + 18, y + 16, 7, COL_RED_BTN);
    gfx_fill_circle(x + 38, y + 16, 7, COL_YEL_BTN);
    gfx_fill_circle(x + 58, y + 16, 7, COL_GRN_BTN);
    int tw = str_len(title) * 8;
    gfx_text(x + (w - tw) / 2, y + 12, title, COL_WIN_TXT, COL_WIN_TITLE);
    gfx_hline(x + 1, y + 31, w - 2, RGB(0x2a, 0x2a, 0x2c));
}

static void draw_terminal_window(void)
{
    int wx = W / 2 - 160, wy = 80, ww = 480, wh = 300;
    draw_window(wx, wy, ww, wh, "Terminal");

    int cx = wx + 12, cy = wy + 40;
    uint32_t tbg = COL_WIN_BG, tp = RGB(0x48, 0xda, 0xff), tt = GFX_WHITE, td = COL_PANEL_DIM;

    terminal_icon = bmp_load(_binary_assets_download_bmp_start);

    if (!terminal_icon)
    {
        gfx_text(40, 40, "BMP LOAD FAILED", GFX_WHITE, RGB(255,0,0));
        return;
    }

    gfx_text(cx, cy, "Last login: Mon Jan 1 00:00:00 2025", td, tbg);
    cy += 14;
    gfx_text(cx, cy, "root", tp, tbg);
    gfx_text(cx + 32, cy, "@", td, tbg);
    gfx_text(cx + 40, cy, "TermuOS", tp, tbg);
    gfx_text(cx + 96, cy, ":~#", td, tbg);
    gfx_text(cx + 120, cy, "uname -a", tt, tbg);
    cy += 14;
    gfx_text(cx, cy, "TermuOS 0.1.0 x86_64", tt, tbg);
    cy += 14;
    gfx_text(cx, cy, "root", tp, tbg);
    gfx_text(cx + 32, cy, "@", td, tbg);
    gfx_text(cx + 40, cy, "TermuOS", tp, tbg);
    gfx_text(cx + 96, cy, ":~#", td, tbg);
    gfx_text(cx + 120, cy, "mem", tt, tbg);
    cy += 14;
    gfx_text(cx, cy, "Total:128MB  Used:4MB  Free:124MB", tt, tbg);
    cy += 14;
    gfx_text(cx, cy, "root", tp, tbg);
    gfx_text(cx + 32, cy, "@", td, tbg);
    gfx_text(cx + 40, cy, "TermuOS", tp, tbg);
    gfx_text(cx + 96, cy, ":~#", td, tbg);
    if ((pit_ticks() / 50) % 2 == 0)
        gfx_fill_rect(cx + 120, cy + 1, 8, 11, tt);

    gfx_blit(terminal_icon, 32, 64);
}

// ─── Notification ─────────────────────────────────────────────────────────────

static void draw_notification(void)
{
    int nw = 280, nh = 64, nx = W - 296, ny = 48;
    gfx_fill_rounded_rect(nx, ny, nw, nh, 12, COL_WIN_TITLE);
    gfx_rounded_rect(nx, ny, nw, nh, 12, COL_DOCK_BORDER);
    gfx_fill_circle(nx + 24, ny + 22, 12, RGB(0x48, 0xda, 0xff));
    gfx_text(nx + 20, ny + 18, "i", COL_PANEL_BG, RGB(0x48, 0xda, 0xff));
    gfx_text(nx + 44, ny + 10, "TermuOS", COL_PANEL_TXT, COL_WIN_TITLE);
    gfx_text(nx + 44, ny + 26, "Welcome to your desktop!", COL_PANEL_DIM, COL_WIN_TITLE);
    gfx_text(nx + nw - 40, ny + 10, "now", COL_PANEL_DIM, COL_WIN_TITLE);
}

// ─── Full redraw ──────────────────────────────────────────────────────────────

static void draw_all(int dock_hover)
{
    draw_wallpaper();
    draw_desktop_icons();
    draw_terminal_window();
    draw_notification();
    draw_dock(dock_hover);
    draw_menubar();
}

// ─── Main loop ────────────────────────────────────────────────────────────────

void desktop_run(void)
{
    W = gfx_width();
    H = gfx_height();
    init_dock_apps();
    mouse_set_screen(W, H);

    int prev_x = W / 2, prev_y = H / 2;
    int dock_hover = -1;
    uint64_t last_clock = 0;

    // Draw immediately
    draw_all(-1);
    gfx_cursor_draw(prev_x, prev_y);

    while (1)
    {
        uint64_t now = pit_ticks();

        mouse_state_t ms = mouse_get();

        int hover = dock_hit_test(ms.x, ms.y);

        if (hover != dock_hover)
            dock_hover = hover;

        // FULL REDRAW EVERY FRAME
        draw_all(dock_hover);

        // Optional clock update
        if (now - last_clock > 100)
            last_clock = now;

        // Draw cursor ONCE after all rendering
        gfx_cursor_draw(ms.x, ms.y);

        gfx_present();

        prev_x = ms.x;
        prev_y = ms.y;

        if (keyboard_haschar())
        {
            char c = keyboard_getchar();

            if (c == 'q')
                break;
        }

        for (volatile int i = 0; i < 50000; i++)
            ;
    }

    gfx_clear(RGB(0x0d, 0x0d, 0x0d));
}