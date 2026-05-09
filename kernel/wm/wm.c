#include "wm.h"
#include "apps.h"
#include "../gfx/gfx.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/input/mouse.h"
#include "../arch/x86_64/pit.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include <stdint.h>
#include <stddef.h>

// Palette
#define COL_DESKTOP_TOP RGB(0x1a, 0x1a, 0x2e)
#define COL_DESKTOP_BOT RGB(0x0f, 0x3d, 0x4a)
#define COL_ACCENT RGB(0x48, 0xda, 0xff)
#define COL_ACCENT2 RGB(0x30, 0x6e, 0xff)

// Window chrome
#define COL_WIN_ACTIVE RGB(0x2c, 0x2c, 0x2e)
#define COL_WIN_INACTIVE RGB(0x1c, 0x1c, 0x1e)
#define COL_WIN_BODY RGB(0x16, 0x16, 0x18)
#define COL_WIN_BORDER_A RGB(0x48, 0xda, 0xff) // active border
#define COL_WIN_BORDER_I RGB(0x3a, 0x3a, 0x3c) // inactive border
#define COL_WIN_TXT_A RGB(0xff, 0xff, 0xff)
#define COL_WIN_TXT_I RGB(0x8e, 0x8e, 0x93)
#define COL_BTN_CLOSE RGB(0xff, 0x5f, 0x57)
#define COL_BTN_MIN RGB(0xff, 0xbd, 0x2e)
#define COL_BTN_MAX RGB(0x28, 0xc8, 0x41)
#define COL_BTN_HOVER RGB(0xff, 0xff, 0xff)

// Taskbar
#define COL_TASKBAR RGB(0x1c, 0x1c, 0x1e)
#define COL_TASKBAR_LINE RGB(0x3a, 0x3a, 0x3c)
#define COL_TASK_ACTIVE RGB(0x30, 0x6e, 0xff)
#define COL_TASK_HOVER RGB(0x2a, 0x2a, 0x2c)
#define COL_TASK_TXT RGB(0xff, 0xff, 0xff)
#define COL_TASK_DIM RGB(0x8e, 0x8e, 0x93)

#define TASKBAR_H 40
#define TASKBAR_Y (H - TASKBAR_H)

// State
static wm_window_t windows[WM_MAX_WINDOWS];
static int n_windows = 0;
static int next_id = 1;
static int W, H;

// Mouse state
static int mx = 0, my = 0;
static int mleft = 0, mleft_prev = 0;
static int mright = 0;

// Drag state
static int drag_win = -1;
static int drag_ox = 0, drag_oy = 0; // offset within titlebar

// Resize state
static int resize_win = -1;
static int resize_ox = 0, resize_oy = 0;
static int resize_ow = 0, resize_oh = 0;

static int focused_id = -1;
static int needs_full_redraw = 1;
static int dock_hover = -1;

// Helpers
static int str_len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}
static void str_cpy(char *d, const char *s, int max)
{
    int i = 0;
    while (s[i] && i < max - 1)
    {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}

static wm_window_t *find(int id)
{
    for (int i = 0; i < n_windows; i++)
        if (windows[i].id == id)
            return &windows[i];
    return NULL;
}

static wm_window_t *find_by_title(const char *title)
{
    for (int i = 0; i < n_windows; i++)
    {
        if (strncmp(windows[i].title, title, 64) == 0)
            return &windows[i];
    }
    return NULL;
}

// Sort windows by z-order (lowest first = drawn first = at back)
static void sort_by_z(void)
{
    for (int i = 0; i < n_windows - 1; i++)
        for (int j = i + 1; j < n_windows; j++)
            if (windows[i].z > windows[j].z)
            {
                wm_window_t tmp = windows[i];
                windows[i] = windows[j];
                windows[j] = tmp;
            }
}

static int max_z(void)
{
    int z = 0;
    for (int i = 0; i < n_windows; i++)
        if (windows[i].z > z)
            z = windows[i].z;
    return z;
}

// Hit test - returns window id at point or -1, topmost first
static int hit_window(int px, int py)
{
    for (int i = n_windows - 1; i >= 0; i--)
    {
        wm_window_t *w = &windows[i];
        if (!(w->flags & WM_FLAG_VISIBLE))
            continue;
        if (w->flags & WM_FLAG_MINIMIZED)
            continue;
        if (px >= w->x && px < w->x + w->w && py >= w->y && py < w->y + w->h)
            return w->id;
    }
    return -1;
}

// Hit test titlebar buttons
#define BTN_NONE 0
#define BTN_CLOSE 1
#define BTN_MIN 2
#define BTN_MAX 3

static int hit_button(wm_window_t *w, int px, int py)
{
    if (py < w->y + 8 || py > w->y + WM_TITLE_H - 8)
        return BTN_NONE;
    int bx = w->x + w->w - 16;
    if (px >= bx - 8 && px <= bx + 8)
        return BTN_CLOSE;
    bx -= 26;
    if (px >= bx - 8 && px <= bx + 8)
        return BTN_MIN;
    bx -= 26;
    if (px >= bx - 8 && px <= bx + 8)
        return BTN_MAX;
    return BTN_NONE;
}

static int in_titlebar(wm_window_t *w, int px, int py)
{
    return px >= w->x && px < w->x + w->w &&
           py >= w->y && py < w->y + WM_TITLE_H;
}

static int in_resize_corner(wm_window_t *w, int px, int py)
{
    if (!(w->flags & WM_FLAG_RESIZABLE))
        return 0;
    return px >= w->x + w->w - 16 && px < w->x + w->w &&
           py >= w->y + w->h - 16 && py < w->y + w->h;
}

// Drawing
static void draw_wallpaper(void)
{
    // Wallpaper between menubar (32px) and taskbar
    int start_y = 32;  // Below menubar
    int end_y = TASKBAR_Y;  // Above taskbar
    int height = end_y - start_y;
    
    for (int i = 0; i < height; i++)
    {
        int t = i * 256 / height;
        uint8_t r = (uint8_t)(0x1a + (int)(0x0f - 0x1a) * t / 256);
        uint8_t g = (uint8_t)(0x1a + (int)(0x3d - 0x1a) * t / 256);
        uint8_t b = (uint8_t)(0x2e + (int)(0x4a - 0x2e) * t / 256);
        gfx_hline(0, start_y + i, W, gfx_rgb(r, g, b));
    }
    // Subtle glow
    for (int r = 150; r > 0; r -= 8)
    {
        uint8_t a = (uint8_t)(r / 8);
        gfx_circle(W / 5, start_y + height * 3 / 4, r, gfx_rgb(0, a, a * 2));
    }
    for (int r = 120; r > 0; r -= 8)
    {
        uint8_t a = (uint8_t)(r / 6);
        gfx_circle(W * 4 / 5, start_y + height / 3, r, gfx_rgb(a, a * 2, 0x4a));
    }
}

static void dock_get_icon_rect(int idx, int *x, int *y, int *w, int *h)
{
    const int icon_w = 80;
    const int padding = 12;
    const int dock_h = 80;
    const int dock_w = 4 * icon_w + padding * 2;
    const int dock_x = (W - dock_w) / 2;
    const int dock_y = TASKBAR_Y - dock_h - 12;
    *x = dock_x + padding + idx * icon_w + 16;
    *y = dock_y + 8;
    *w = 48;
    *h = 48;
}

static int hit_dock(int px, int py)
{
    for (int i = 0; i < 4; i++)
    {
        int x, y, w, h;
        dock_get_icon_rect(i, &x, &y, &w, &h);
        if (px >= x && px < x + w && py >= y && py < y + h)
            return i;
    }
    return -1;
}

static void dock_click(int idx)
{
    static const char *dock_titles[4] = {"Terminal", "Files", "Clock", "About TermuOS"};
    if (idx < 0 || idx >= 4)
        return;
    wm_window_t *w = find_by_title(dock_titles[idx]);
    if (!w)
    {
        // Launch new app
        switch (idx)
        {
        case 0: apps_launch_terminal(); break;
        case 1: apps_launch_files(); break;
        case 2: apps_launch_clock(); break;
        case 3: apps_launch_about(); break;
        }
    }
    else
    {
        // Toggle existing window
        if (w->flags & WM_FLAG_MINIMIZED)
            wm_restore(w->id);
        else if (w->id == focused_id)
            wm_minimize(w->id);
        else
            wm_focus(w->id);
    }
}

static void draw_dock(void)
{
    const int icon_w = 80;
    const int padding = 12;
    const int dock_h = 80;
    const int dock_w = 4 * icon_w + padding * 2;
    const int dock_x = (W - dock_w) / 2;
    const int dock_y = TASKBAR_Y - dock_h - 12;

    gfx_fill_rounded_rect(dock_x, dock_y, dock_w, dock_h, 16, RGB(0x24, 0x24, 0x2c));
    gfx_rounded_rect(dock_x, dock_y, dock_w, dock_h, 16, RGB(0x3a, 0x3a, 0x3c));

    const char *icons[4] = {">_", "[]", "⌂", "i"};
    const char *labels[4] = {"Terminal", "Files", "Clock", "About"};

    for (int i = 0; i < 4; i++)
    {
        int x, y, w, h;
        dock_get_icon_rect(i, &x, &y, &w, &h);
        uint32_t bg = (i == dock_hover) ? RGB(0x30, 0x6e, 0xff) : RGB(0x16, 0x16, 0x18);
        uint32_t fg = (i == dock_hover) ? RGB(0xff, 0xff, 0xff) : RGB(0xff, 0xff, 0xff);
        uint32_t border = (i == dock_hover) ? RGB(0x9e, 0xca, 0xff) : RGB(0x3a, 0x3a, 0x3c);

        gfx_fill_rounded_rect(x, y, w, h, 12, bg);
        gfx_rounded_rect(x, y, w, h, 12, border);

        int sw = str_len(icons[i]) * 8;
        gfx_text(x + (w - sw) / 2, y + 16, icons[i], fg, bg);
        int lw = str_len(labels[i]) * 8;
        gfx_text(x + (w - lw) / 2, y + h - 14, labels[i], COL_TASK_DIM, bg);
    }
}

static void draw_menubar(void)
{
    // Top menu bar
    gfx_fill_rect(0, 0, W, 32, RGB(0x2c, 0x2c, 0x2e));
    gfx_hline(0, 31, W, RGB(0x3a, 0x3a, 0x3c));
    gfx_fill_circle(16, 16, 10, RGB(0x48, 0xda, 0xff));
    gfx_text(12, 12, "T", RGB(0x2c, 0x2c, 0x2e), 0);
}

static void draw_taskbar(void)
{
    // Background
    gfx_fill_rect(0, TASKBAR_Y, W, TASKBAR_H, COL_TASKBAR);
    gfx_hline(0, TASKBAR_Y, W, COL_TASKBAR_LINE);

    // Start button / logo
    gfx_fill_rounded_rect(8, TASKBAR_Y + 6, 36, 28, 8, COL_ACCENT2);
    gfx_text(14, TASKBAR_Y + 14, "T", RGB(0xff, 0xff, 0xff), COL_ACCENT2);

    // window buttons in taskbar
    int tx = 54;
    for (int i = 0; i < n_windows; i++)
    {
        wm_window_t *w = &windows[i];
        if (!(w->flags & WM_FLAG_VISIBLE))
            continue;

        int tw = 140;
        int focused = (w->id == focused_id);
        uint32_t tbg = focused ? COL_TASK_ACTIVE : COL_TASK_HOVER;

        // Check hover
        if (mx >= tx && mx < tx + tw && my >= TASKBAR_Y && my < H)
            tbg = focused ? COL_TASK_ACTIVE : RGB(0x3a, 0x3a, 0x3c);

        gfx_fill_rounded_rect(tx, TASKBAR_Y + 6, tw, 28, 6, tbg);

        // Minimized indicator dot
        if (w->flags & WM_FLAG_MINIMIZED)
            gfx_fill_circle(tx + 8, TASKBAR_Y + 20, 3, RGB(0xff, 0xbd, 0x2e));
        else if (focused)
            gfx_fill_circle(tx + 8, TASKBAR_Y + 20, 3, COL_ACCENT);

        // Title (truncated)
        char trunc[14];
        int tl = str_len(w->title);
        if (tl > 3)
        {
            for (int k = 0; k < 12; k++)
                trunc[k] = w->title[k];
            trunc[12] = '.';
            trunc[13] = 0;
        }
        else
        {
            str_cpy(trunc, w->title, 14);
        }
        gfx_text(tx + 16, TASKBAR_Y + 14, trunc, COL_TASK_TXT, tbg);
        tx += tw + 4;
    }

    // Clock (right side)
    uint64_t t = pit_ticks(), s = t / 100, m = (s / 60) % 60, h = (s / 3600) % 24;
    char clk[6];
    clk[0] = '0' + h / 10;
    clk[1] = '0' + h % 10;
    clk[2] = ':';
    clk[3] = '0' + m / 10;
    clk[4] = '0' + m % 10;
    clk[5] = 0;
    gfx_text(W - 52, TASKBAR_Y + 14, clk, COL_TASK_TXT, COL_TASKBAR);
}

static void draw_window_chrome(wm_window_t *w)
{
    int active = (w->id == focused_id);
    uint32_t title_bg = active ? COL_WIN_ACTIVE : COL_WIN_INACTIVE;
    uint32_t border = active ? COL_WIN_BORDER_A : COL_WIN_BORDER_I;
    uint32_t txt_col = active ? COL_WIN_TXT_A : COL_WIN_TXT_I;

    // Drop shadow
    for (int s = 6; s > 0; s--)
    {
        uint8_t alpha = (uint8_t)(s * 6);
        gfx_rounded_rect(w->x + s, w->y + s, w->w, w->h, 12, gfx_rgb(0, 0, 0));
    }

    // Window body
    gfx_fill_rounded_rect(w->x, w->y, w->w, w->h, 12, COL_WIN_BODY);

    // Title bar
    gfx_fill_rounded_rect(w->x, w->y, w->w, WM_TITLE_H, 12, title_bg);
    gfx_fill_rect(w->x, w->y + 12, w->w, WM_TITLE_H - 12, title_bg);

    // Border
    gfx_rounded_rect(w->x, w->y, w->w, w->h, 12, border);

    // Title separator
    gfx_hline(w->x + 1, w->y + WM_TITLE_H, w->w - 2, active ? RGB(0x3a, 0x3a, 0x4e) : RGB(0x2a, 0x2a, 0x2c));

    // TItle text centred
    int tlen = str_len(w->title) * 8;
    gfx_text(w->x + (w->w - tlen) / 2, w->y + 10, w->title, txt_col, title_bg);

    // Traffic light buttons (right side, macOS style)
    int bx = w->x + w->w - 16;
    int by = w->y + WM_TITLE_H / 2;

    // Close
    gfx_fill_circle(bx, by, 7, COL_BTN_CLOSE);
    if (mx >= bx - 8 && mx <= bx + 8 && my >= w->y && my <= w->y + WM_TITLE_H)
        gfx_text(bx - 3, by - 4, "x", RGB(0x80, 0x00, 0x00), COL_BTN_CLOSE);
    bx -= 26;

    // Minimise
    gfx_fill_circle(bx, by, 7, COL_BTN_MIN);
    if (mx >= bx - 8 && mx <= bx + 8 && my >= w->y && my <= w->y + WM_TITLE_H)
        gfx_text(bx - 3, by - 4, "-", RGB(0x80, 0x60, 0x00), COL_BTN_MIN);
    bx -= 26;

    // Maximise
    gfx_fill_circle(bx, by, 7, COL_BTN_MAX);
    if (mx >= bx - 8 && mx <= bx + 8 && my >= w->y && my <= w->y + WM_TITLE_H)
        gfx_text(bx - 3, by - 4, "+", RGB(0x00, 0x60, 0x00), COL_BTN_MAX);

    // Resize grip (bottom-right corner)
    if (w->flags & WM_FLAG_RESIZABLE)
    {
        int rx = w->x + w->w - 4, ry = w->y + w->h - 4;
        for (int d = 4; d <= 14; d += 5)
        {
            gfx_line(rx - d, w->y + w->h - 2, w->x + w->w - 2, ry - d, RGB(0x5a, 0x5a, 0x5e));
        }
    }
}

static void draw_window(wm_window_t *w)
{
    if (!(w->flags & WM_FLAG_VISIBLE))
        return;
    if (w->flags & WM_FLAG_MINIMIZED)
        return;

    // Draw chrome background first
    draw_window_chrome(w);

    // Set clip to window client area
    gfx_set_clip(w->x + WM_BORDER, w->y + WM_TITLE_H, w->w - WM_BORDER * 2, w->h - WM_TITLE_H - WM_BORDER);

    // Draw client content
    if (w->draw)
        w->draw(w->id, w->x + WM_BORDER, w->y + WM_TITLE_H, w->w - WM_BORDER * 2, w->h - WM_TITLE_H - WM_BORDER);
    else
        gfx_fill_rect(w->x + WM_BORDER, w->y + WM_TITLE_H, w->w - WM_BORDER * 2, w->h - WM_TITLE_H - WM_BORDER, COL_WIN_BODY);

    gfx_clear_clip();
}

static void composite(void)
{
    sort_by_z();
    draw_wallpaper();
    draw_menubar();
    for (int i = 0; i < n_windows; i++)
        draw_window(&windows[i]);
    draw_dock();
    draw_taskbar();
}

// API
void wm_init(void)
{
    n_windows = 0;
    focused_id = -1;
    W = gfx_width();
    H = gfx_height();
    mouse_set_screen(W, H);
    kprintf("WM: init %dx%d\n", W, H);
}

int wm_create(const char *title, int x, int y, int w, int h, uint32_t flags, wm_draw_fn draw)
{
    if (n_windows >= WM_MAX_WINDOWS)
        return -1;

    wm_window_t *win = &windows[n_windows++];
    win->id = next_id++;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->flags = flags | WM_FLAG_VISIBLE | WM_FLAG_CLOSEABLE;
    win->z = max_z() + 1;
    win->draw = draw;
    win->animating = 0;
    str_cpy(win->title, title, 64);

    wm_focus(win->id);
    needs_full_redraw = 1;
    return win->id;
}

void wm_destroy(int id)
{
    for (int i = 0; i < n_windows; i++)
    {
        if (windows[i].id == id)
        {
            for (int j = i; j < n_windows - 1; j++)
                windows[j] = windows[j + 1];
            n_windows--;
            if (focused_id == id)
                focused_id = -1;
            needs_full_redraw = 1;
            return;
        }
    }
}

void wm_focus(int id)
{
    wm_window_t *w = find(id);
    if (!w)
        return;
    // Unfocus previous
    wm_window_t *prev = find(focused_id);
    if (prev)
        prev->flags &= ~WM_FLAG_FOCUSED;
    focused_id = id;
    w->flags |= WM_FLAG_FOCUSED;
    wm_raise(id);
    needs_full_redraw = 1;
}

void wm_raise(int id)
{
    wm_window_t *w = find(id);
    if (w)
    {
        w->z = max_z() + 1;
        needs_full_redraw = 1;
    }
}

void wm_minimize(int id)
{
    wm_window_t *w = find(id);
    if (!w)
        return;
    w->flags |= WM_FLAG_MINIMIZED;
    if (focused_id == id)
        focused_id = -1;
    needs_full_redraw = 1;
}

void wm_maximize(int id)
{
    wm_window_t *w = find(id);
    if (!w || (w->flags & WM_FLAG_MAXIMIZED))
        return;
    w->prev_x = w->x;
    w->prev_y = w->y;
    w->prev_w = w->w;
    w->prev_h = w->h;
    w->x = 0;
    w->y = 0;
    w->w = W;
    w->h = TASKBAR_Y;
    w->flags |= WM_FLAG_MAXIMIZED;
    needs_full_redraw = 1;
}

void wm_restore(int id)
{
    wm_window_t *w = find(id);
    if (!w)
        return;
    if (w->flags & WM_FLAG_MAXIMIZED)
    {
        w->x = w->prev_x;
        w->y = w->prev_y;
        w->w = w->prev_w;
        w->h = w->prev_h;
        w->flags &= ~WM_FLAG_MAXIMIZED;
    }
    if (w->flags & WM_FLAG_MINIMIZED)
    {
        w->flags &= ~WM_FLAG_MINIMIZED;
        wm_focus(id);
    }
    needs_full_redraw = 1;
}

void wm_move(int id, int x, int y)
{
    wm_window_t *w = find(id);
    if (w)
    {
        w->x = x;
        w->y = y;
        needs_full_redraw = 1;
    }
}

void wm_resize(int id, int nw, int nh)
{
    wm_window_t *w = find(id);
    if (!w)
        return;
    if (nw < WM_MIN_W)
        nw = WM_MIN_W;
    if (nh < WM_MIN_H)
        nh = WM_MIN_H;
    w->w = nw;
    w->h = nh;
    needs_full_redraw = 1;
}

void wm_set_title(int id, const char *title)
{
    wm_window_t *w = find(id);
    if (w)
    {
        str_cpy(w->title, title, 64);
        needs_full_redraw = 1;
    }
}

void wm_redraw(int id)
{
    (void)id;
    needs_full_redraw = 1;
}
void wm_redraw_all(void) { needs_full_redraw = 1; }

wm_window_t *wm_get(int id) { return find(id); }
int wm_focused(void) { return focused_id; }

// Input handling

static void handle_mouse(void)
{
    if (!mouse_moved())
        return;

    mouse_state_t ms = mouse_get();
    mx = ms.x;
    my = ms.y;
    
    // Clamp mouse to screen
    if (mx < 0) mx = 0;
    if (mx >= W) mx = W - 1;
    if (my < 0) my = 0;
    if (my >= H) my = H - 1;
    
    mleft_prev = mleft;
    mleft = ms.left;
    mright = ms.right;

    int clicked = mleft && !mleft_prev; // rising edge

    // Dragging
    if (drag_win >= 0)
    {
        if (mleft)
        {
            wm_window_t *w = find(drag_win);
            if (w && !(w->flags & WM_FLAG_MAXIMIZED))
            {
                w->x = mx - drag_ox;
                w->y = my - drag_oy;
                // Clamp
                if (w->y < 0)
                    w->y = 0;
                if (w->y + WM_TITLE_H > TASKBAR_Y)
                    w->y = TASKBAR_Y - WM_TITLE_H;
                needs_full_redraw = 1;
            }
        }
        else
        {
            drag_win = -1;
        }
        return;
    }

    // Resizing
    if (resize_win >= 0)
    {
        if (mleft)
        {
            wm_window_t *w = find(resize_win);
            if (w)
            {
                int nw = resize_ow + (mx - resize_ox);
                int nh = resize_oh + (my - resize_oy);
                wm_resize(resize_win, nw, nh);
            }
        }
        else
        {
            resize_win = -1;
        }
        return;
    }

    // Hover state for dock
    int new_dock_hover = hit_dock(mx, my);
    if (new_dock_hover != dock_hover)
    {
        dock_hover = new_dock_hover;
        needs_full_redraw = 1;
    }

    // Click handling
    if (clicked)
    {
        int dock_idx = hit_dock(mx, my);
        if (dock_idx >= 0)
        {
            dock_click(dock_idx);
            return;
        }

        // Check taskbar window buttons
        if (my >= TASKBAR_Y)
        {
            int tx = 54;
            for (int i = 0; i < n_windows; i++)
            {
                wm_window_t *w = &windows[i];
                if (!(w->flags & WM_FLAG_VISIBLE))
                    continue;
                if (mx >= tx && mx < tx+140)
                {
                    if (w->flags & WM_FLAG_MINIMIZED)
                        wm_restore(w->id);
                    else if (w->id == focused_id)
                        wm_minimize(w->id);
                    else
                        wm_focus(w->id);
                    return;
                }
                tx += 144;
            }
            return;
        }

        int wid = hit_window(mx, my);
        if (wid < 0) return;

        wm_window_t *w = find(wid);
        if (!w) return;

        // Focus on click
        if (wid != focused_id) wm_focus(wid);

        // Button clicks
        int btn = hit_button(w, mx, my);
        if (btn == BTN_CLOSE) { wm_destroy(wid); return; }
        if (btn == BTN_MIN) { wm_minimize(wid); return; }
        if (btn == BTN_MAX)
        {
            if (w->flags & WM_FLAG_MAXIMIZED)
                wm_restore(wid);
            else
                wm_maximize(wid);
            return;
        }

        // Start drag
        if (in_titlebar(w, mx, my))
        {
            // Double click = maximise/restore
            drag_win = wid;
            drag_ox = mx - w->x;
            drag_oy = my - w->y;
            return;
        }

        // Start resize
        if (in_resize_corner(w, mx, my))
        {
            resize_win = wid;
            resize_ox = mx;
            resize_oy = my;
            resize_ow = w->w;
            resize_oh = w->h;
            return;
        }
    }
    // Hovering - need redraw for button highlights
    needs_full_redraw = 1;
}

// Main loop
void wm_run(void)
{
    int prev_mx = W/2, prev_my = H/2;
    uint64_t last_tick = 0;

    // Initial composite
    composite();
    gfx_cursor_draw(prev_mx, prev_my);
    gfx_present();

    while (1)
    {
        // Clock update every second
        uint64_t now = pit_ticks();
        if (now - last_tick > 100)
        {
            last_tick = now;
            // Just redraw taskbar clock area
            draw_taskbar();
            gfx_cursor_draw(mx, my);
            gfx_present();
        }

        handle_mouse();

        if (needs_full_redraw)
        {
            composite();
            gfx_cursor_draw(mx, my);
            prev_mx = mx; prev_my = my;
            needs_full_redraw = 0;
            gfx_present();
        }
        else if (mx != prev_mx || my != prev_my)
        {
            gfx_cursor_draw(mx, my);
            prev_mx = mx; prev_my = my;
            gfx_present();
        }

        if (keyboard_haschar())
        {
            char c = keyboard_getchar();
            if (c == 'q') break;
            // Forward to focused window (future use)
        }

        __asm__ volatile("hlt");
    }
}