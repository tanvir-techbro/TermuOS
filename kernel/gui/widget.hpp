#pragma once
#include <stdint.h>

class Window;

class Widget
{
public:
    int x, y, w, h; // relative to window content area

    Widget(int x, int y, int w, int h)
        : x(x), y(y), w(w), h(h) {}

    virtual ~Widget() = default;

    // Called to draw the widget inside its parent window
    virtual void draw(Window *win) = 0;

    // Called on mouse click - screen coords. Returns true if consumed.
    virtual bool on_click(int mx, int my) { (void)mx; (void)my; return false; }

    // Called on keypress when this widget is focused
    virtual void on_key(char c) { (void)c; }

    // Whether this widget can recieve keyboard focus
    virtual bool focusable() const { return false; }
};
