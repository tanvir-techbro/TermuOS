#pragma once
#include <stdint.h>
#include "window.hpp"
#include "launcher.hpp"
#include "../../assets/icon_logo.h"
#include "context_menu.hpp"

#define DESKTOP_MAX_WINDOWS  16
#define DESKTOP_MAX_APPS     16
#define DESKTOP_TASKBAR_H    52
#define DESKTOP_ICON_PAD     6
#define APP_ICON_W           32
#define APP_ICON_H           32

// ─── App descriptor ───────────────────────────────────────────────────────────

class App {
public:
    const char    *name;
    const uint8_t *icon_bmp;

    App(const char *name, const uint8_t *icon_bmp = nullptr)
        : name(name), icon_bmp(icon_bmp) {}

    virtual ~App() = default;
    virtual void launch() = 0;
};

// ─── Desktop ──────────────────────────────────────────────────────────────────

class Desktop {
public:
    static Desktop &instance();

    void init(uint32_t bg_colour);
    void add_app(App *app);

    // Open a managed window — Desktop takes ownership
    int  open_window(Window *win);
    void close_window(int handle);
    Window *get_window(int handle);

    void redraw();
    void run();
    void handle_click(int mx, int my);

    int screen_w() const;
    int screen_h() const;
    int taskbar_y() const { return screen_h() - DESKTOP_TASKBAR_H; }

private:
    Desktop() = default;

    void draw_background();
    void draw_taskbar();
    void draw_windows();

    void handle_right_click(int mx, int my);

    uint32_t  _bg_colour  = 0;
    bool      _bg_loaded  = false;
    App      *_apps[DESKTOP_MAX_APPS]       = {};
    int       _app_count  = 0;
    Window   *_windows[DESKTOP_MAX_WINDOWS] = {};
    int       _focused    = -1;
    int       _hover_app  = -1;

    ContextMenu _context_menu;
    int         _context_app = -1; // which app was right clicked

    Launcher _launcher;
    bool     _launcher_open = false;

    static constexpr int LOGO_X = 8;
    static constexpr int LOGO_W = 32;
    static constexpr int LOGO_H = 32;
};
