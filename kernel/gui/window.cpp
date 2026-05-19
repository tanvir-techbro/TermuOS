#include "window.hpp"
extern "C" {
#include "../drivers/video/gfx.h"
#include "../drivers/video/fb.h"
}

Window::Window(const char *title, int x, int y, int w, int h,
               uint32_t bg, uint32_t border, uint32_t title_bg, uint32_t title_fg)
    : x(x), y(y), w(w), h(h)
    , title(title)
    , bg(bg), border(border), title_bg(title_bg), title_fg(title_fg)
    , _widget_count(0), _focused_widget(-1)
{
    for (int i = 0; i < WIN_MAX_WIDGETS; i++)
        _widgets[i] = nullptr;
}

// chrome

void Window::draw_chrome()
{
    // outer border
    gfx_draw_rect(x, y, w, h, border);

    // title bar background
    gfx_fill_rect(x + 1, y + 1, w - 2, WIN_TITLE_H, title_bg);

    // title text
    if (title)
        gfx_draw_string(x + 5, y + 3, title, title_fg, title_bg);

    // seperator line
    gfx_draw_hline(x + 1, y + 1 + WIN_TITLE_H, w - 2, border);

    // content background
    gfx_fill_rect(content_x(), content_y(), content_w(), content_h(), bg);
}

// widgets

void Window::add_widget(Widget *widget)
{
    if (_widget_count < WIN_MAX_WIDGETS)
        _widgets[_widget_count++] = widget;
}

void Window::draw_widgets()
{
    for (int i = 0; i < _widget_count; i++)
    if (_widgets[i])
        _widgets[i]->draw(this);
}

void Window::draw()
{
    draw_chrome();
    draw_widgets();
}

// Input

void Window::on_key(char c)
{
    if (_focused_widget >= 0 && _widgets[_focused_widget])
        _widgets[_focused_widget]->on_key(c);
}

bool Window::on_click(int mx, int my)
{
    for (int i = 0; i < _widget_count; i++)
    {
        if (!_widgets[i]) continue;
        if (_widgets[i]->on_click(mx, my))
        {
            if (_widgets[i]->focusable())
                _focused_widget = i;
            return true;
        }
    }
    return false;
}
