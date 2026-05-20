#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus

#define LAUNCHER_COLS       4
#define LAUNCHER_ICON_SIZE  48
#define LAUNCHER_ICON_PAD   16
#define LAUNCHER_LABEL_H    16
#define LAUNCHER_ITEM_H     (LAUNCHER_ICON_SIZE + LAUNCHER_LABEL_H + 8)
#define LAUNCHER_ITEM_W     (LAUNCHER_ICON_SIZE + LAUNCHER_ICON_PAD)
#define LAUNCHER_PAD        20
#define LAUNCHER_TITLE_H    36

class App;

class Launcher
{
public:
    Launcher();

    void show(int screen_w, int screen_h, int taskbar_y);
    void hide();
    bool visible() const { return _visible; }

    void draw();

    // returns true if click consumed
    bool on_click(int mx, int my);

    void set_apps(App **apps, int count);

private:
    App     **_apps;
    int       _count;
    int       _x, _y, _w, _h;
    bool      _visible;
    int       _hovered;

    int rows() const;
    void item_pos(int i, int &ox, int &oy) const;
};

#endif
