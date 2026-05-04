#include "desktop.h"
#include "../gfx/gfx.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/input/mouse.h"
#include "../arch/x86_64/pit.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

// ─── TermuOS Desktop ──────────────────────────────────────────────────────────
// Style: macOS menubar + Pop!_OS colour palette + rounded dock
//
// Layout:
//   Top bar  (32px)  — apple-style menubar with clock + menu items
//   Desktop  (fill)  — gradient wallpaper + icons
//   Dock     (72px)  — centred, rounded, frosted-glass style

// ─── Palette (Pop!_OS + macOS vibes) ─────────────────────────────────────────
#define COL_BG_TOP RGB(0x1a, 0x1a, 0x2e)   // deep navy
#define COL_BG_MID RGB(0x16, 0x21, 0x3e)   // dark blue
#define COL_BG_BOT RGB(0x0f, 0x3d, 0x4a)   // teal-black
#define COL_ACCENT RGB(0x48, 0xda, 0xff)   // Pop!_OS cyan
#define COL_ACCENT2 RGB(0xfa, 0xb3, 0x87)  // warm orange
#define COL_PANEL_BG RGB(0x1c, 0x1c, 0x1e) // macOS dark bar
#define COL_PANEL_TXT RGB(0xff, 0xff, 0xff)
#define COL_PANEL_DIM RGB(0x8e, 0x8e, 0x93)
#define COL_DOCK_BG RGB(0x2c, 0x2c, 0x2e) // dark glass
#define COL_DOCK_BORDER RGB(0x3a, 0x3a, 0x3c)
#define COL_WIN_BG RGB(0x1c, 0x1c, 0x1e)
#define COL_WIN_TITLE RGB(0x2c, 0x2c, 0x2e)
#define COL_WIN_TXT RGB(0xff, 0xff, 0xff)
#define COL_RED_BTN RGB(0xff, 0x5f, 0x57)
#define COL_YEL_BTN RGB(0xff, 0xbd, 0x2e)
#define COL_GRN_BTN RGB(0x28, 0xc8, 0x41)
#define COL_ICON_TXT RGB(0xff, 0xff, 0xff)
#define COL_ICON_SHD RGB(0x00, 0x00, 0x00)
#define COL_SEL RGB(0x30, 0x6e, 0xff) // Pop!_OS blue select

// ─── Helpers ──────────────────────────────────────────────────────────────────

static int W, H;

static void gradient_rect(int x, int y, int w, int h,
                          uint32_t top, uint32_t bot)
{
    // Extract components (assume RGB888 in known bit positions)
    // We'll just interpolate the raw values — works if shifts are consistent
    // Get channel values from gfx_rgb by decomposing known colours
    // Simpler: just do vertical gradient by interpolating r,g,b separately

    // Decompose top colour (we know how we built it via RGB macro)
    // top = R<<rs | G<<gs | B<<bs — but we don't have rs/gs/bs here
    // Workaround: use a series of fill_rects with lerped RGB values

    uint8_t tr = (top >> 16) & 0xff; // won't work for all bit orders
    // Let's just use gfx_rgb for clean colours and lerp via RGB tuples
    // Store as separate params — caller passes RGB() values but we need
    // to extract. Since we control the palette, hardcode the lerp.
    (void)top;
    (void)bot;

    // Just use fill_rect with solid colour for simplicity — OS gradient
    // achieved by drawing multiple thin horizontal bands
    for (int i = 0; i < h; i++)
    {
        int t = i * 256 / h;
        // Lerp between COL_BG_TOP and COL_BG_BOT
        uint8_t r = (uint8_t)(0x1a + (int)(0x0f - 0x1a) * t / 256);
        uint8_t g = (uint8_t)(0x1a + (int)(0x3d - 0x1a) * t / 256);
        uint8_t b = (uint8_t)(0x2e + (int)(0x4a - 0x2e) * t / 256);
        gfx_hline(x, y + i, w, gfx_rgb(r, g, b));
    }
}

static void text_shadow(int x, int y, const char *s, uint32_t fg)
{
    gfx_text(x + 1, y + 1, s, COL_ICON_SHD, 0);
    gfx_text(x, y, s, fg, 0);
}

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

// ─── Wallpaper ────────────────────────────────────────────────────────────────

static void draw_wallpaper(void)
{
    gradient_rect(0, 32, W, H - 32 - 72, 0, 0);

    // Subtle accent glow circles (Pop!_OS vibes)
    // Bottom-left glow
    for (int r = 120; r > 0; r -= 4)
    {
        uint8_t a = (uint8_t)(30 - r / 5);
        gfx_circle(W / 4, H * 3 / 4, r, gfx_rgb(0x00, a * 2, a * 3));
    }
    // Top-right glow
    for (int r = 100; r > 0; r -= 4)
    {
        uint8_t a = (uint8_t)(25 - r / 5);
        gfx_circle(W * 3 / 4, H / 3, r, gfx_rgb(a * 2, a * 3, 0x4a + a));
    }

    // "TermuOS" watermark centre
    int tx = W / 2 - 4 * 8;
    int ty = H / 2 - 20;
    gfx_text(tx + 1, ty + 1, "TermuOS", gfx_rgb(0, 0, 0), 0);
    gfx_text(tx, ty, "TermuOS", gfx_rgb(0x48, 0xda, 0xff), 0);
    // Version below
    text_centre(0, ty + 14, W, "0.1.0", gfx_rgb(0x48, 0x8a, 0xa0), 0);
}

// ─── Top menu bar ─────────────────────────────────────────────────────────────

static char clock_str[16] = "00:00";

static void update_clock(void)
{
    uint64_t t = pit_ticks();
    uint64_t s = t / 100;
    uint64_t m = (s / 60) % 60;
    uint64_t h = (s / 3600) % 24;
    // Simple itoa
    clock_str[0] = '0' + h / 10;
    clock_str[1] = '0' + h % 10;
    clock_str[2] = ':';
    clock_str[3] = '0' + m / 10;
    clock_str[4] = '0' + m % 10;
    clock_str[5] = 0;
}

static void draw_menubar(void)
{
    // Background
    gfx_fill_rect(0, 0, W, 32, COL_PANEL_BG);
    // Bottom separator
    gfx_hline(0, 31, W, gfx_rgb(0x3a, 0x3a, 0x3c));

    // Apple-style logo (filled circle with T)
    gfx_fill_circle(20, 16, 10, COL_ACCENT);
    gfx_text(16, 12, "T", COL_PANEL_BG, 0);

    // Menu items
    const char *menus[] = {"File", "Edit", "View", "Terminal", "Help", NULL};
    int mx = 46;
    for (int i = 0; menus[i]; i++)
    {
        gfx_text(mx, 12, menus[i], COL_PANEL_TXT, COL_PANEL_BG);
        mx += str_len(menus[i]) * 8 + 16;
    }

    // Right side: clock + wifi + battery icons
    update_clock();
    int cw = str_len(clock_str) * 8;
    gfx_text(W - cw - 12, 12, clock_str, COL_PANEL_TXT, COL_PANEL_BG);

    // Separator before clock
    gfx_vline(W - cw - 20, 6, 20, gfx_rgb(0x3a, 0x3a, 0x3c));

    // WiFi icon (simple arc stack)
    int wx = W - cw - 50;
    gfx_fill_circle(wx, 22, 2, COL_PANEL_TXT);
    gfx_circle(wx, 22, 5, COL_PANEL_TXT);
    gfx_circle(wx, 22, 9, COL_PANEL_TXT);
    // Clip upper half of wifi by overdrawing
    gfx_fill_rect(wx - 12, 6, 24, 16, COL_PANEL_BG);
    gfx_fill_rect(wx - 12, 6, 24, 5, COL_PANEL_BG);

    // Battery icon
    int bx = W - cw - 80;
    gfx_rect(bx - 10, 8, 18, 10, COL_PANEL_TXT);
    gfx_fill_rect(bx + 8, 11, 3, 4, COL_PANEL_TXT);
    gfx_fill_rect(bx - 9, 9, 14, 8, COL_GRN_BTN); // full battery
}

// ─── Dock icons ───────────────────────────────────────────────────────────────

typedef struct
{
    const char *label;
    uint32_t colour;
    uint32_t icon_col;
    const char *symbol;
} dock_app_t;

#define N_APPS 6

// Pre-computed colours for dock (can't use RGB() in static initializers)
// These are filled at init time
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

static void draw_dock_icon(int cx, int by, const dock_app_t *app, int hover)
{
    int sz = hover ? 58 : 52;
    int x = cx - sz / 2;
    int y = by - sz;

    // Icon shadow
    gfx_fill_rounded_rect(x + 3, y + 3, sz, sz, 12, gfx_rgb(0, 0, 0));

    // Icon background
    gfx_fill_rounded_rect(x, y, sz, sz, 12, app->colour);

    // Highlight edge (top)
    gfx_rounded_rect(x, y, sz, sz, 12, gfx_rgb(0x5a, 0x5a, 0x5a));

    // Symbol centred
    int sw = str_len(app->symbol) * 8;
    gfx_text(x + (sz - sw) / 2, y + sz / 2 - 4, app->symbol, app->icon_col, app->colour);

    // Label below dock
    int lw = str_len(app->label) * 8;
    if (hover)
    {
        // Tooltip bubble
        gfx_fill_rounded_rect(cx - lw / 2 - 6, by + 6, lw + 12, 18, 6, COL_PANEL_BG);
        gfx_rounded_rect(cx - lw / 2 - 6, by + 6, lw + 12, 18, 6, COL_DOCK_BORDER);
        gfx_text(cx - lw / 2, by + 10, app->label, COL_PANEL_TXT, COL_PANEL_BG);
    }

    // Running dot
    gfx_fill_circle(cx, by + 6, 3, COL_ACCENT);
}

static void draw_dock(int hovered)
{
    int dock_h = 72;
    int icon_w = 64;
    int padding = 12;
    int n = N_APPS;
    int dock_w = n * icon_w + padding * 2;
    int dock_x = (W - dock_w) / 2;
    int dock_y = H - dock_h;

    // Dock background — frosted glass style
    gfx_fill_rounded_rect(dock_x, dock_y + 8, dock_w, dock_h - 8, 16, COL_DOCK_BG);
    gfx_rounded_rect(dock_x, dock_y + 8, dock_w, dock_h - 8, 16, COL_DOCK_BORDER);

    // Subtle top highlight
    gfx_hline(dock_x + 16, dock_y + 9, dock_w - 32, gfx_rgb(0x5a, 0x5a, 0x5e));

    // Icons
    for (int i = 0; i < n; i++)
    {
        int cx = dock_x + padding + i * icon_w + icon_w / 2;
        int by = dock_y + 4;
        draw_dock_icon(cx, by, &dock_apps[i], i == hovered);
    }
}

// ─── Desktop icons ────────────────────────────────────────────────────────────

static void draw_desktop_icon(int x, int y, const char *label,
                              uint32_t bg, const char *sym)
{
    // Icon
    gfx_fill_rounded_rect(x, y, 60, 60, 14, bg);
    gfx_rounded_rect(x, y, 60, 60, 14, gfx_rgb(0x5a, 0x5a, 0x5a));
    int sw = str_len(sym) * 8;
    gfx_text(x + (60 - sw) / 2, y + 22, sym, GFX_WHITE, bg);

    // Label with shadow
    int lw = str_len(label) * 8;
    text_shadow(x + (60 - lw) / 2, y + 66, label, COL_ICON_TXT);
}

static void draw_desktop_icons(void)
{
    int start_x = 24;
    int start_y = 48;
    int step = 90;

    draw_desktop_icon(start_x, start_y,
                      "Home", RGB(0x30, 0x6e, 0xff), "~");
    draw_desktop_icon(start_x, start_y + step,
                      "Docs", RGB(0x20, 0x8a, 0xd0), "D");
    draw_desktop_icon(start_x, start_y + step * 2,
                      "Trash", RGB(0x6e, 0x6e, 0x7e), "X");
}

// ─── Notification ─────────────────────────────────────────────────────────────

static void draw_notification(const char *title, const char *msg)
{
    int nw = 280, nh = 64;
    int nx = W - nw - 16;
    int ny = 48;

    gfx_fill_rounded_rect(nx, ny, nw, nh, 12, COL_WIN_TITLE);
    gfx_rounded_rect(nx, ny, nw, nh, 12, COL_DOCK_BORDER);
    // App icon dot
    gfx_fill_circle(nx + 24, ny + 22, 12, COL_ACCENT);
    gfx_text(nx + 20, ny + 18, "i", COL_PANEL_BG, COL_ACCENT);
    // Title
    gfx_text(nx + 44, ny + 10, title, COL_PANEL_TXT, COL_WIN_TITLE);
    // Message
    gfx_text(nx + 44, ny + 26, msg, COL_PANEL_DIM, COL_WIN_TITLE);
    // Timestamp
    gfx_text(nx + nw - 40, ny + 10, "now", COL_PANEL_DIM, COL_WIN_TITLE);
}

// ─── Terminal window ──────────────────────────────────────────────────────────

static void draw_window(int x, int y, int w, int h, const char *title)
{
    // Shadow
    gfx_fill_rounded_rect(x + 4, y + 6, w, h, 14, gfx_rgb(0, 0, 0));

    // Window body
    gfx_fill_rounded_rect(x, y, w, h, 14, COL_WIN_BG);
    gfx_rounded_rect(x, y, w, h, 14, gfx_rgb(0x3a, 0x3a, 0x3c));

    // Title bar
    gfx_fill_rounded_rect(x, y, w, 32, 14, COL_WIN_TITLE);
    gfx_fill_rect(x, y + 14, w, 18, COL_WIN_TITLE); // flatten bottom corners

    // Traffic lights
    gfx_fill_circle(x + 18, y + 16, 7, COL_RED_BTN);
    gfx_fill_circle(x + 38, y + 16, 7, COL_YEL_BTN);
    gfx_fill_circle(x + 58, y + 16, 7, COL_GRN_BTN);

    // Title
    int tw = str_len(title) * 8;
    gfx_text(x + (w - tw) / 2, y + 12, title, COL_WIN_TXT, COL_WIN_TITLE);

    // Title bar separator
    gfx_hline(x + 1, y + 31, w - 2, gfx_rgb(0x2a, 0x2a, 0x2c));
}

static void draw_terminal_window(void)
{
    int wx = W / 2 - 160, wy = 80, ww = 480, wh = 300;
    draw_window(wx, wy, ww, wh, "Terminal");

    // Content area
    int cx = wx + 12, cy = wy + 40;
    uint32_t tbg = COL_WIN_BG;
    uint32_t tprompt = COL_ACCENT;
    uint32_t ttext = GFX_WHITE;
    uint32_t tdim = COL_PANEL_DIM;

    gfx_text(cx, cy, "Last login: Mon Jan  1 00:00:00 2025", tdim, tbg);
    cy += 14;
    gfx_text(cx, cy, "root", tprompt, tbg);
    gfx_text(cx + 32, cy, "@", tdim, tbg);
    gfx_text(cx + 40, cy, "TermuOS", tprompt, tbg);
    gfx_text(cx + 96, cy, ":~#", tdim, tbg);
    gfx_text(cx + 120, cy, "uname -a", ttext, tbg);
    cy += 14;
    gfx_text(cx, cy, "TermuOS 0.1.0 x86_64", ttext, tbg);
    cy += 14;
    gfx_text(cx, cy, "root", tprompt, tbg);
    gfx_text(cx + 32, cy, "@", tdim, tbg);
    gfx_text(cx + 40, cy, "TermuOS", tprompt, tbg);
    gfx_text(cx + 96, cy, ":~#", tdim, tbg);
    gfx_text(cx + 120, cy, "mem", ttext, tbg);
    cy += 14;
    gfx_text(cx, cy, "Total: 128MB  Used: 4MB  Free: 124MB", ttext, tbg);
    cy += 14;
    gfx_text(cx, cy, "root", tprompt, tbg);
    gfx_text(cx + 32, cy, "@", tdim, tbg);
    gfx_text(cx + 40, cy, "TermuOS", tprompt, tbg);
    gfx_text(cx + 96, cy, ":~#", tdim, tbg);
    // Blinking cursor
    uint64_t tick = pit_ticks();
    if ((tick / 50) % 2 == 0)
        gfx_fill_rect(cx + 120, cy + 1, 8, 11, ttext);
}

// ─── Full desktop draw ────────────────────────────────────────────────────────

static void draw_desktop(int hovered_dock)
{
    draw_wallpaper();
    draw_desktop_icons();
    draw_terminal_window();
    draw_notification("TermuOS", "Welcome to your desktop!");
    draw_dock(hovered_dock);
    draw_menubar(); // draw last so it's on top
}

// ─── Main loop ────────────────────────────────────────────────────────────────

void desktop_run(void)
{
    W = gfx_width();
    H = gfx_height();
    init_dock_apps();

    // Set mouse screen bounds to match desktop resolution
    mouse_set_screen(W, H);

    int hovered = -1;
    uint64_t last_tick = 0;

    // Mouse and cursor state
    mouse_state_t mouse_state;
    int cursor_x = W / 2; // Start at centre
    int cursor_y = H / 2;

    // Initial draw
    draw_desktop(hovered);
    // Draw initial cursor
    gfx_cursor_draw(cursor_x, cursor_y);

    // Simple event loop — keyboard navigation of dock
    // Left/Right arrows move dock focus, Enter "launches" app
    while (1)
    {
        // Redraw clock every second
        uint64_t now = pit_ticks();
        if (now - last_tick > 100)
        {
            last_tick = now;
            // Redraw just the menubar clock area
            draw_menubar();
            // Redraw cursor blink in terminal
            draw_terminal_window();
        }

        // Non-blocking key check
        if (keyboard_haschar())
        {
            char c = keyboard_getchar();
            if (c == 'q' || c == 'Q')
                break; // quit to shell

            // Arrow-like: a/d to move dock selection
            if (c == 'd' || c == '\t')
            {
                hovered = (hovered + 1) % N_APPS;
                draw_desktop(hovered);
            }
            else if (c == 'a')
            {
                hovered = (hovered - 1 + N_APPS) % N_APPS;
                draw_desktop(hovered);
            }
            else if (c == '\n' && hovered >= 0)
            {
                // Flash selected icon
                draw_desktop(hovered);
            }
        }

        // Poll mouse for movement
        if (mouse_moved())
        {
            // erase old cursor
            gfx_cursor_erase(cursor_x, cursor_y);

            // Get new mouse state
            mouse_state = mouse_get();
            cursor_x = mouse_state.x;
            cursor_y = mouse_state.y;

            // Draw new cursor
            gfx_cursor_draw(cursor_x, cursor_y);
        }

        // Handle mouse clicks (e.g., dock interaction)
        if (mouse_state.left) // Assuming left button press
        {
            // Check if cursor is over dock icons
            int dock_y_start = H - 72;
            int dock_y_end = H;
            if (cursor_y >= dock_y_start && cursor_y <= dock_y_end)
            {
                // Calculate which icon was clicked
                int icon_width = 64;
                int padding = 12;
                int dock_start_x = (W - (N_APPS * icon_width + padding * 2)) / 2 + padding;
                for (int i = 0; i < N_APPS; i++)
                {
                    int icon_x_start = dock_start_x + i * icon_width;
                    int icon_x_end = icon_x_start + icon_width;
                    if (cursor_x >= icon_x_start && cursor_x <= icon_x_end)
                    {
                        // Handle click on dock icon i (e.g., set hovered or "launch")
                        hovered = i;
                        draw_desktop(hovered);
                        break;
                    }
                }
            }
            // Reset button state or debounce as needed
        }

        __asm__ volatile("hlt");
    }

    // Erase cursor before exiting
    gfx_cursor_erase(cursor_x, cursor_y);

    // Restore terminal
    gfx_clear(RGB(0x0d, 0x0d, 0x0d));
}