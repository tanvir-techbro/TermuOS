#include "term_app.hpp"
#include "desktop.hpp"
extern "C" {
#include "../drivers/video/gfx.h"
#include "../drivers/video/fb.h"
#include "../lib/printf.h"
#include "../shell/shell.h"
}

// ─── Static instance ──────────────────────────────────────────────────────────

TerminalApp *TerminalApp::_instance = nullptr;

// ─── TermWindow ───────────────────────────────────────────────────────────────

TerminalApp::TermWindow::TermWindow(TerminalApp *app, int x, int y, int w, int h)
    : Window("Terminal", x, y, w, h,
             fb_colour(0x0d, 0x0d, 0x0d),
             fb_colour(80,  120, 220),
             fb_colour(40,  70,  160),
             fb_colour(255, 255, 255))
    , app(app)
{}

void TerminalApp::TermWindow::on_key(char c)
{
    app->do_key(c);
}

// ─── Constructor ──────────────────────────────────────────────────────────────

TerminalApp::TerminalApp()
    : App("Terminal", nullptr)
    , _cx(0), _cy(0), _input_len(0), _handle(-1)
{
    _instance = this;
    for (int i = 0; i < TERM_ROWS * TERM_COLS; i++) _buf[i] = ' ';
    for (int i = 0; i < TERM_COLS; i++) _input[i] = '\0';
}

// ─── Buffer ops ───────────────────────────────────────────────────────────────

void TerminalApp::scroll()
{
    for (int row = 0; row < TERM_ROWS - 1; row++)
        for (int col = 0; col < TERM_COLS; col++)
            _buf[row * TERM_COLS + col] = _buf[(row + 1) * TERM_COLS + col];
    for (int col = 0; col < TERM_COLS; col++)
        _buf[(TERM_ROWS - 1) * TERM_COLS + col] = ' ';
    _cy = TERM_ROWS - 1;
}

void TerminalApp::putchar(char c)
{
    if (c == '\014') // form feed = clear screen
    {
        for (int i = 0; i < TERM_ROWS * TERM_COLS; i++) _buf[i] = ' ';
        _cx = 0; _cy = 0;
        return;
    }
    if (c == '\n' || c == '\r') {
        _cx = 0; _cy++;
        if (_cy >= TERM_ROWS) scroll();
        return;
    }
    if (c == '\b') {
        if (_cx > 0) { _cx--; _buf[_cy * TERM_COLS + _cx] = ' '; }
        return;
    }
    if (c == '\t') {
        int spaces = 4 - (_cx % 4);
        for (int i = 0; i < spaces; i++) putchar(' ');
        return;
    }
    if (_cx >= TERM_COLS) { _cx = 0; _cy++; }
    if (_cy >= TERM_ROWS) scroll();
    _buf[_cy * TERM_COLS + _cx++] = c;
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void TerminalApp::do_draw()
{
    Window *win = Desktop::instance().get_window(_handle);
    if (!win) return;

    uint32_t fg = fb_colour(0x00, 0xff, 0x88);
    uint32_t bg = fb_colour(0x0d, 0x0d, 0x0d);

    int ox = win->content_x() + TERM_PAD;
    int oy = win->content_y() + TERM_PAD;

    // Fill content
    gfx_fill_rect(win->content_x(), win->content_y(),
                  win->content_w(), win->content_h(), bg);

    // Draw characters
    for (int row = 0; row < TERM_ROWS; row++)
    {
        int py = oy + row * GFX_FONT_H;
        for (int col = 0; col < TERM_COLS; col++)
        {
            char ch = _buf[row * TERM_COLS + col];
            gfx_draw_char(ox + col * GFX_FONT_W, py, ch ? ch : ' ', fg, bg);
        }
    }

    // Cursor
    int cx = ox + _cx * GFX_FONT_W;
    int cy = oy + _cy * GFX_FONT_H;
    gfx_fill_rect(cx, cy, GFX_FONT_W, GFX_FONT_H, fg);
    char cc = _buf[_cy * TERM_COLS + _cx];
    if (cc && cc != ' ')
        gfx_draw_char(cx, cy, cc, bg, fg);
}

// ─── Key handler ──────────────────────────────────────────────────────────────

void TerminalApp::print_prompt()
{
    const char *cwd = shell_get_cwd();
    puts("root@TermuOS:");
    puts(cwd);
    puts("# ");
}

static void term_putchar_redirect(char ch)
{
    TerminalApp::_instance->putchar(ch);
}

void TerminalApp::do_key(char c)
{
    if (c == '\n' || c == '\r')
    {
        putchar('\n');
        _input[_input_len] = '\0';
        kprintf_set_output(term_putchar_redirect);
        shell_run_command(_input);
        kprintf_set_output(nullptr);
        _input_len = 0;
        _input[0]  = '\0';
        print_prompt();
    }
    else if (c == '\b')
    {
        if (_input_len > 0) {
            _input[--_input_len] = '\0';
            putchar('\b');
        }
    }
    else
    {
        if (_input_len < TERM_COLS - 1) {
            _input[_input_len++] = c;
            _input[_input_len]   = '\0';
            putchar(c);
        }
    }

    Window *win = Desktop::instance().get_window(_handle);
    if (win) win->draw_chrome();
    do_draw();
}

// ─── Static puts ──────────────────────────────────────────────────────────────

void TerminalApp::puts(const char *s)
{
    if (!_instance || !s) return;
    while (*s) _instance->putchar(*s++);
}

// ─── Launch ───────────────────────────────────────────────────────────────────

void TerminalApp::launch()
{
    // Reset buffer
    for (int i = 0; i < TERM_ROWS * TERM_COLS; i++) _buf[i] = ' ';
    _cx = 0; _cy = 0; _input_len = 0;

    Desktop &desk = Desktop::instance();
    int sw = desk.screen_w();
    int sh = desk.screen_h();

    int w = TERM_COLS * GFX_FONT_W + TERM_PAD * 2 + 2;
    int h = TERM_ROWS * GFX_FONT_H + TERM_PAD * 2 + WIN_TITLE_H + 2;
    int x = (sw - w) / 2;
    int y = (sh - DESKTOP_TASKBAR_H - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    TermWindow *win = new TermWindow(this, x, y, w, h);
    _handle = desk.open_window(win);

    puts("TermuOS 0.1.0 -- type 'help' for commands.\n");
    print_prompt();
    do_draw();
    desk.redraw();
}