#pragma once
#include <stdint.h>
#include "widget.hpp"

#define WIN_MAX_WIDGETS 16
#define WIN_TITLE_H     20 // GFX_FONT_H (16) + 4

class Window
{
public:
    int x, y, w, h;
    const char *title;

    uint32_t bg;
    uint32_t border;
    uint32_t title_bg;
    uint32_t title_fg;

    Window(const char *title, int x, int y, int w, int h,
           uint32_t bg, uint32_t border, uint32_t title_bg, uint32_t title_fg);

    virtual ~Window() = default;

    // Draw chrome (border title bar, content background)
    void draw_chrome();

    // Draw all widgets
    void draw_widgets();

    // Full draw - chrome + widgets
    void draw();

    // Add a widget (does not take ownership - caller manages lifetime)
    void add_widget(Widget *widget);

    // Route a keypress to the focused widget
    virtual void on_key(char c);

    // Route a click to widgets - returns true if consumed
    virtual bool on_click(int mx, int my);

    // Content area dimensions
    int content_x() const { return x + 1; }
    int content_y() const { return y + 1 + WIN_TITLE_H; }
    int content_w() const { return w - 2; }
    int content_h() const { return h - 2 - WIN_TITLE_H; }

protected:
    Widget *_widgets[WIN_MAX_WIDGETS];
    int     _widget_count;
    int     _focused_widget; // index into _widgets, -1 if none
};
