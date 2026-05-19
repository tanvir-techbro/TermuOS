#pragma once
#include <stdint.h>
#include "widget.hpp"

// Theme

struct Theme
{
    uint32_t fg;
    uint32_t bg;
    uint32_t border;
    uint32_t accent;
    uint32_t hover;
    uint32_t pressed;

    static Theme &get();
    static void set_default();
};

// Label

class Label : public Widget
{
public:
    const char *text;
    uint32_t    fg;

    Label(int x, int y, const char *text, uint32_t fg = 0);
    void draw(Window *win) override;
};

// Button

class Button : public Widget
{
public:
    const char *text;
    bool pressed;
    void (*on_press)(Button *);

    Button(int x, int y, int w, int h, const char *text, void (*on_press)(Button *) = nullptr);
    void draw(Window *win) override;
    bool on_click(int mx, int my) override;
    bool focusable() const override { return true; }

private:
    // screen coords of last drawn position (set during draw)
    int _sx, _sy;
};

// Textbox

#define TEXTBOX_MAX 128

class Textbox : public Widget
{
public:
    char buf[TEXTBOX_MAX];
    int len;
    bool focused;
    void (*on_submit)(Textbox *);

    Textbox(int x, int y, int w, void (*on_submit)(Textbox *) = nullptr);
    void draw(Window *win) override;
    void on_key(char c) override;
    bool on_click(int mx, int my) override;
    bool focusable() const override { return true; }

    void clear();

private:
    int _sx, _sy;
};

// List

#define LIST_MAX_ITEMS 64

class List : public Widget
{
public:
    const char *items[LIST_MAX_ITEMS];
    int count;
    int selected;
    int scroll;
    void (*on_select)(List *, int index);

    List(int x, int y, int w, int h, void (*on_select)(List *, int) = nullptr);
    void draw(Window *win) override;
    bool on_click(int mx, int my) override;
    bool focusable() const override { return true; }

    void add_item(const char *item);
    void clear_items();

private:
    int _sx, _sy;
};
