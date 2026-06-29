#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../ipc/port.h"

// limits
#define WM_MAX_WINDOWS 16
#define WM_PORT_NAME   "wm" // apps connect to this port
#define WM_TITLE_MAX   64

// IPC message codes (wm.code field)
// app->compositor
#define WM_CREATE_WINDOW    0x01 // payload: wm_create_req_t
#define WM_DESTROY_WINDOW   0x02 // payload: wm_win_req_t
#define WM_DRAW_RECT        0x03 // payload: wm_draw_rect_t
#define WM_DRAW_BITMAP      0x04 // payload: wm_draw_bitmap_t
#define WM_SET_TITLE        0x05 // payload: wm_set_title_t
#define WM_MOVE_WINDOW      0x06 // payload: wm_move_t
#define WM_RAISE_WINDOW     0x07 // payload: wm_win_req_t

// compositor->app (events)
#define WM_EVENT_MOUSE      0x80 // payload: wm_mouse_event_t
#define WM_EVENT_KEY        0x81 // payload: wm_key_event_t
#define WM_EVENT_CLOSE      0x82 // no payload - window closed
#define WM_REPLY_CREATE     0x83 // payload: wm_create_reply_t

// IPC payload structs
typedef struct
{
    int x, y;
    int width, height;
    char title[WM_TITLE_MAX];
    char reply_port[32]; // apps event port name
} wm_create_req_t;

typedef struct
{
    uint32_t win_id;
} wm_create_reply_t;

typedef struct
{
    uint32_t win_id;   
} wm_win_req_t;

typedef struct
{
    uint32_t win_id;
    int x, y; // relative to window client area
    int width, height;
    uint32_t colour; // 0xRRGGBB
} wm_draw_rect_t;

typedef struct
{
    uint32_t win_id;
    int x, y;
    int width, height;
    uint32_t *pixels; // caller allocates; wm copies
} wm_draw_bitmap_t;

typedef struct
{
    uint32_t win_id;
    char title[WM_TITLE_MAX];
} wm_set_title_t;

typedef struct
{
    uint32_t win_id;
    int x, y;
} wm_move_t;

typedef struct
{
    int x, y;
    int dx, dy;
    uint8_t left, right, middle;
} wm_mouse_event_t;

typedef struct
{
    uint8_t scancode;
    char ascii;
    uint8_t pressed;
} wm_key_event_t;

// internal window record (compositor-private)
#define WIN_FLAG_VISIBLE    (1 << 0)
#define WIN_FLAG_FOCUSED    (1 << 1)
#define WIN_FLAG_DRAGGING   (1 << 2)

#define WM_TITLEBAR_H   24  // pixels
#define WM_BORDER_W     2

typedef struct wm_window {
    uint32_t id;
    int x, y;
    int width, height;
    uint32_t flags;
    char title[WM_TITLE_MAX];
    port_t *event_port;
    uint32_t *back_buf;
} wm_window_t;

// API
void wm_init(void);
void wm_thread_entry(void); // pass to thread_create()

// internal helpers exposed for draw.c
wm_window_t *wm_find(uint32_t id);
int wm_window_count(void);
wm_window_t *wm_get(int index);
wm_window_t *wm_focused(void);
void wm_set_focus(wm_window_t *w);
