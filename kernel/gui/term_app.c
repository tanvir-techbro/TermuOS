#include "term_app.h"
#include "desktop.h"
#include "ui.h"
#include "../drivers/video/gfx.h"
#include "../drivers/video/fb.h"
#include "../shell/shell.h"
#include "../lib/printf.h"
#include "../mm/heap.h"
#include "../../assets/icon_terminal.h"
#include <stddef.h>
#include <stdint.h>

// ─── Config ───────────────────────────────────────────────────────────────────

#define TERM_FG fb_colour(0x00, 0xff, 0x88)
#define TERM_BG fb_colour(0x0d, 0x0d, 0x0d)
#define TERM_PAD 4

// ─── State ────────────────────────────────────────────────────────────────────

// Simple line buffer for the terminal window
#define TERM_COLS 80
#define TERM_ROWS 20
#define TERM_BUF (TERM_COLS * TERM_ROWS)

static char _buf[TERM_BUF];
static int _cx = 0, _cy = 0; // cursor col, row
static int _handle = -1;
static window_t *_win = NULL;

// forward dec
static void term_app_print_prompt(void);

// ─── Buffer ops ───────────────────────────────────────────────────────────────

static void term_scroll(void)
{
    // Shift every row up by one
    for (int row = 0; row < TERM_ROWS - 1; row++)
        for (int col = 0; col < TERM_COLS; col++)
            _buf[row * TERM_COLS + col] = _buf[(row + 1) * TERM_COLS + col];

    // Clear last row
    for (int col = 0; col < TERM_COLS; col++)
        _buf[(TERM_ROWS - 1) * TERM_COLS + col] = ' ';

    _cy = TERM_ROWS - 1;
}

static void term_putchar(char c)
{
    if (c == '\n' || c == '\r')
    {
        _cx = 0;
        _cy++;
        if (_cy >= TERM_ROWS)
            term_scroll();
        return;
    }
    if (c == '\b')
    {
        if (_cx > 0)
        {
            _cx--;
            _buf[_cy * TERM_COLS + _cx] = ' ';
        }
        return;
    }
    if (c == '\t')
    {
        int spaces = 4 - (_cx % 4);
        for (int i = 0; i < spaces; i++)
            term_putchar(' ');
        return;
    }

    if (_cx >= TERM_COLS)
    {
        _cx = 0;
        _cy++;
    }
    if (_cy >= TERM_ROWS)
        term_scroll();

    _buf[_cy * TERM_COLS + _cx] = c;
    _cx++;
}

// ─── Draw callback ────────────────────────────────────────────────────────────

static void term_draw(desk_window_t *dw)
{
    win_draw(&dw->win); // redraw chrome first

    int ox = dw->win.x + 1 + TERM_PAD;
    int oy = dw->win.y + 1 + WIN_TITLE_H + TERM_PAD;

    // Fill content area
    win_fill_rect(&dw->win, 0, 0, win_content_w(&dw->win), win_content_h(&dw->win), TERM_BG);

    // Draw each row
    for (int row = 0; row < TERM_ROWS; row++)
    {
        int py = oy + row * GFX_FONT_H;
        for (int col = 0; col < TERM_COLS; col++)
        {
            char c = _buf[row * TERM_COLS + col];
            if (!c)
                c = ' ';
            gfx_draw_char(ox + col * GFX_FONT_W, py, c, TERM_FG, TERM_BG);
        }
    }

    // Draw cursor
    int cx = ox + _cx * GFX_FONT_W;
    int cy = oy + _cy * GFX_FONT_H;
    gfx_fill_rect(cx, cy, GFX_FONT_W, GFX_FONT_H, TERM_FG);
    char cc = _buf[_cy * TERM_COLS + _cx];
    if (cc && cc != ' ')
        gfx_draw_char(cx, cy, cc, TERM_BG, TERM_FG);
}

// ─── Key callback ─────────────────────────────────────────────────────────────

// Shell input buffer
static char _input[TERM_COLS];
static int _input_len = 0;

// kprintf redirect — write to our buffer instead of terminal
// We do this by hooking terminal_putchar via a function pointer
// For now we provide a simple puts that feeds into our buffer
void term_app_puts(const char *s)
{
    while (*s)
    {
        term_putchar(*s++);
    }
    term_draw(desktop_get_window(_handle));
}

static void term_key(desk_window_t *dw, char c)
{
    if (c == '\n' || c == '\r')
    {
        term_putchar('\n');
        _input[_input_len] = '\0';
        kprintf_set_output(term_putchar);
        // Run shell command
        shell_run_command(_input);
        kprintf_set_output(NULL);
        term_app_print_prompt();
        _input_len = 0;
        _input[0] = '\0';
    }
    else if (c == '\b')
    {
        if (_input_len > 0)
        {
            _input_len--;
            _input[_input_len] = '\0';
            term_putchar('\b');
        }
    }
    else
    {
        if (_input_len < TERM_COLS - 1)
        {
            _input[_input_len++] = c;
            _input[_input_len] = '\0';
            term_putchar(c);
        }
    }

    win_draw(&dw->win);
    term_draw(dw);
}

// ─── Launch ───────────────────────────────────────────────────────────────────

static void term_app_print_prompt(void)
{
    term_app_puts("root@TermuOS:");
    term_app_puts(shell_get_cwd());
    term_app_puts("# ");
}

void term_app_launch(void)
{
    // Clear buffer
    for (int i = 0; i < TERM_BUF; i++)
        _buf[i] = ' ';
    _cx = 0;
    _cy = 0;
    _input_len = 0;

    int sw = (int)fb_get()->width;
    int sh = (int)fb_get()->height;

    // Open a window sized for 80x24 at 8px wide, 16px tall + padding
    int w = TERM_COLS * GFX_FONT_W + TERM_PAD * 2 + 2;
    int h = TERM_ROWS * GFX_FONT_H + TERM_PAD * 2 + WIN_TITLE_H + 2;

    // Centre on screen
    int x = (sw - w) / 2;
    int y = (sh - DESKTOP_TASKBAR_H - h) / 2;
    if (y < 0) y = 0;
    if (x < 0) x = 0;

    _handle = desktop_open_window("Terminal", x, y, w, h, term_draw, term_key);
    _win = &desktop_get_window(_handle)->win;

    term_app_puts("TermuOS 0.1.0 -- type 'help' for commands.\n");
    term_app_print_prompt();
    desktop_redraw();
}

// ─── App descriptor ───────────────────────────────────────────────────────────

static const app_t _term_app_desc = {
    .name = "Terminal",
    .icon_bmp = icon_terminal_bmp, // replace with your BMP data when ready
    .launch = term_app_launch,
};

const app_t *term_app_get(void) { return &_term_app_desc; }