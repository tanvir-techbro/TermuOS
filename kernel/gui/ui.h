#pragma once
#include <stdint.h>
#include "window.h"

// ─── UI colours (defaults, apps can override) ─────────────────────────────────

typedef struct
{
    uint32_t fg;
    uint32_t bg;
    uint32_t border;
    uint32_t accent;
    uint32_t hover;
    uint32_t pressed;
} ui_theme_t;

extern ui_theme_t ui_theme;

void ui_theme_default(void);

// ─── Label ────────────────────────────────────────────────────────────────────

typedef struct
{
    int x, y; // relative to window content area
    const char *text;
    uint32_t fg;
} ui_label_t;

void ui_draw_label(window_t *win, ui_label_t *l);

// ─── Button ───────────────────────────────────────────────────────────────────

typedef struct
{
    int x, y, w, h; // relative to window content area
    const char *text;
    int pressed; // set by ui_button_hit()
} ui_button_t;

void ui_draw_button(window_t *win, ui_button_t *b);

// Returns 1 if (mx, my) is inside the button (screen coords)
int ui_button_hit(window_t *win, ui_button_t *b, int mx, int my);

// ─── Textbox ──────────────────────────────────────────────────────────────────

#define UI_TEXTBOX_MAX 128

typedef struct
{
    int x, y, w; // relative to window content area, h = 1 line
    char buf[UI_TEXTBOX_MAX];
    int len;
    int focused;
} ui_textbox_t;

void ui_draw_textbox(window_t *win, ui_textbox_t *t);

// Feed a character into the textbox (handles backspace)
void ui_textbox_input(ui_textbox_t *t, char c);

// ─── List ─────────────────────────────────────────────────────────────────────

#define UI_LIST_MAX_ITEMS 64

typedef struct
{
    int x, y, w, h; // relative to window content area
    const char *items[UI_LIST_MAX_ITEMS];
    int count;
    int selected;
    int scroll; // first visible item index
} ui_list_t;

void ui_draw_list(window_t *win, ui_list_t *l);

// Returns 1 and updates selected if (mx, my) hit an item
int ui_list_hit(window_t *win, ui_list_t *l, int mx, int my);