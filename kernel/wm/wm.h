#pragma once
#include <stdint.h>
#include <stddef.h>

#define WM_MAX_WINDOWS  16
#define WM_TITLE_H      32
#define WM_BORDER       2
#define WM_MIN_W        120
#define WM_MIN_H        80

// Window flags
#define WM_FLAG_VISIBLE     (1<<0)
#define WM_FLAG_FOCUSED     (1<<1)
#define WM_FLAG_DRAGGING    (1<<2)
#define WM_FLAG_RESIZING    (1<<3)
#define WM_FLAG_MINIMIZED   (1<<4)
#define WM_FLAG_MAXIMIZED   (1<<5)
#define WM_FLAG_CLOSEABLE   (1<<6)
#define WM_FLAG_RESIZABLE   (1<<7)

typedef void (*wm_draw_fn)(int id, int x, int y, int w, int h);

typedef struct
{
    int id;
    int x, y, w, h; // position and size
    int prev_x, prev_y; // saved pos for restore from max
    int prev_w, prev_h;
    char title[64];
    uint32_t flags;
    int z; // z-order (higher = on top)
    wm_draw_fn draw; // callback to draw client area
    // Animation state
    int anim_x, anim_y; // current animated position
    int anim_w, anim_h;
    int animating;
} wm_window_t;

// Init
void wm_init(void);

// Window management
int wm_create(const char *title, int x, int y, int w, int h, uint32_t flags, wm_draw_fn draw);
void wm_destroy(int id);
void wm_focus(int id);
void wm_raise(int id);
void wm_minimize(int id);
void wm_maximize(int id);
void wm_restore(int id);
void wm_move(int id, int x, int y);
void wm_resize(int id, int w, int h);
void wm_set_title(int id, const char *title);
void wm_redraw(int id); // mark window dirty
void wm_redraw_all(void);

// Main loop - call this instead of desktop_run
void wm_run(void);

// Query
wm_window_t *wm_get(int id);
int wm_focused(void);
