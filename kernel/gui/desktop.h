#pragma once
#include <stdint.h>
#include "app.h"
#include "window.h"

// ─── Desktop config ───────────────────────────────────────────────────────────

#define DESKTOP_TASKBAR_H    48
#define DESKTOP_MAX_WINDOWS  16
#define DESKTOP_MAX_APPS     16
#define DESKTOP_ICON_PAD     8

// ─── Managed window ───────────────────────────────────────────────────────────

typedef struct desk_window_s {
    window_t    win;
    int         open;
    int         focused;
    void       (*on_draw)(struct desk_window_s *dw);
    void       (*on_key)(struct desk_window_s *dw, char c);
} desk_window_t;

// ─── Desktop API ──────────────────────────────────────────────────────────────

void desktop_init(uint32_t bg_colour);
void desktop_add_app(const app_t *app);

int  desktop_open_window(const char *title, int x, int y, int w, int h,
                         void (*on_draw)(desk_window_t *dw),
                         void (*on_key)(desk_window_t *dw, char c));

void           desktop_close_window(int handle);
desk_window_t *desktop_get_window(int handle);
void           desktop_redraw(void);
void           desktop_run(void);