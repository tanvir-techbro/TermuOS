#include "wm.h"
#include "draw.h"
#include "../drivers/video/fb.h"
#include "../drivers/input/mouse.h"
#include "../drivers/input/keyboard.h"
#include "../ipc/port.h"
#include "../sched/scheduler.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"

/* ══════════════════════════════════════════════════════════════
 *  Colour scheme
 * ══════════════════════════════════════════════════════════════ */
#define COL_DESKTOP     0x1A1A2E   /* dark navy                  */
#define COL_TASKBAR     0x16213E
#define COL_TB_FOCUSED  0x0F3460   /* active titlebar            */
#define COL_TB_UNFOCUS  0x2C3E50   /* inactive titlebar          */
#define COL_TB_TEXT_F   0xFFFFFF
#define COL_TB_TEXT_U   0xAAAAAA
#define COL_WIN_BG      0xECF0F1   /* client area background     */
#define COL_BORDER_F    0x0F3460
#define COL_BORDER_U    0x7F8C8D
#define COL_CLOSE_BTN   0xE74C3C
#define COL_CLOSE_HOV   0xFF6B6B
#define COL_TASKBAR_TXT 0xECF0F1

#define TASKBAR_H       28         /* pixels at bottom           */

/* ══════════════════════════════════════════════════════════════
 *  Window table
 * ══════════════════════════════════════════════════════════════ */
static wm_window_t windows[WM_MAX_WINDOWS];
static int         win_count  = 0;
static uint32_t    next_id    = 1;
static int         focused_idx = -1;  /* index into windows[]   */

/* Z-order: windows[0] is bottom, windows[win_count-1] is top.   */

/* ─── public accessors used by draw.c ──────────────────────── */
wm_window_t *wm_find(uint32_t id)
{
    for (int i = 0; i < win_count; i++)
        if (windows[i].id == id) return &windows[i];
    return NULL;
}
int          wm_window_count(void) { return win_count; }
wm_window_t *wm_get(int i)        { return &windows[i]; }
wm_window_t *wm_focused(void)
{
    if (focused_idx < 0 || focused_idx >= win_count) return NULL;
    return &windows[focused_idx];
}
void wm_set_focus(wm_window_t *w)
{
    /* Clear old focus flag */
    if (focused_idx >= 0 && focused_idx < win_count)
        windows[focused_idx].flags &= ~WIN_FLAG_FOCUSED;
    if (!w) { focused_idx = -1; return; }
    for (int i = 0; i < win_count; i++) {
        if (&windows[i] == w) {
            focused_idx = i;
            w->flags |= WIN_FLAG_FOCUSED;
            return;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Geometry helpers — declared before any function that uses them
 * ══════════════════════════════════════════════════════════════ */

static inline int full_w(wm_window_t *w) { return w->width  + 2 * WM_BORDER_W; }
static inline int full_h(wm_window_t *w) { return w->height + WM_TITLEBAR_H + WM_BORDER_W; }
static inline int decor_x(wm_window_t *w) { return w->x - WM_BORDER_W; }
static inline int decor_y(wm_window_t *w) { return w->y - WM_TITLEBAR_H; }

/* ══════════════════════════════════════════════════════════════
 *  Rendering
 * ══════════════════════════════════════════════════════════════ */

static void render_window(wm_window_t *w)
{
    int focused = (w->flags & WIN_FLAG_FOCUSED) != 0;
    int dx = decor_x(w);
    int dy = decor_y(w);
    int fw = full_w(w);

    /* ── border ── */
    uint32_t border_col = focused ? draw_rgb(0x0F,0x34,0x60)
                                  : draw_rgb(0x7F,0x8C,0x8D);
    draw_rect_outline(dx, dy, fw, full_h(w), WM_BORDER_W, border_col);

    /* ── titlebar ── */
    uint32_t tb_col = focused ? draw_rgb(0x0F,0x34,0x60)
                              : draw_rgb(0x2C,0x3E,0x50);
    draw_rect(dx, dy, fw, WM_TITLEBAR_H, tb_col);

    /* close button — 14×14 centred vertically in titlebar */
    int btn_size = 14;
    int btn_x = dx + fw - btn_size - 5;
    int btn_y = dy + (WM_TITLEBAR_H - btn_size) / 2;
    draw_rect(btn_x, btn_y, btn_size, btn_size, draw_rgb(0xE7,0x4C,0x3C));
    /* × symbol */
    uint32_t x_col = draw_rgb(0xFF,0xFF,0xFF);
    draw_text(btn_x + 3, btn_y + 3, "x", x_col, 0, 1);

    /* title text — clipped so it doesn't overlap close button */
    uint32_t txt_col = focused ? draw_rgb(0xFF,0xFF,0xFF)
                               : draw_rgb(0xAA,0xAA,0xAA);
    int title_x = dx + 8;
    int title_y = dy + (WM_TITLEBAR_H - FONT_H) / 2;
    int clip_end = btn_x - 4;
    draw_textf(title_x, title_y,
               title_x, clip_end - title_x,
               w->title, txt_col, 0, 1);

    /* ── client area background (only if no back_buf yet) ── */
    if (!w->back_buf)
        draw_rect(w->x, w->y, w->width, w->height,
                  draw_rgb(0xEC,0xF0,0xF1));
    else
        draw_blit(w->x, w->y, w->width, w->height,
                  w->back_buf, w->width);
}

static void render_taskbar(void)
{
    struct limine_framebuffer *fb = fb_get();
    if (!fb) return;
    int sw = (int)fb->width;
    int sh = (int)fb->height;

    draw_rect(0, sh - TASKBAR_H, sw, TASKBAR_H, draw_rgb(0x16,0x21,0x3E));
    draw_hline(0, sh - TASKBAR_H, sw, draw_rgb(0x0F,0x34,0x60));

    /* OS name on left */
    draw_text(8, sh - TASKBAR_H + (TASKBAR_H - FONT_H) / 2,
              "TermuOS", draw_rgb(0xEC,0xF0,0xF1), 0, 1);

    /* window buttons */
    int bx = 80;
    for (int i = 0; i < win_count; i++) {
        wm_window_t *w = &windows[i];
        if (!(w->flags & WIN_FLAG_VISIBLE)) continue;
        int focused = (w->flags & WIN_FLAG_FOCUSED) != 0;
        uint32_t bb = focused ? draw_rgb(0x0F,0x34,0x60)
                               : draw_rgb(0x2C,0x3E,0x50);
        draw_rect(bx, sh - TASKBAR_H + 4, 120, TASKBAR_H - 8, bb);
        draw_textf(bx + 4, sh - TASKBAR_H + (TASKBAR_H - FONT_H) / 2,
                   bx + 4, 112,
                   w->title, draw_rgb(0xEC,0xF0,0xF1), 0, 1);
        bx += 128;
    }
}

static void render_desktop(void)
{
    struct limine_framebuffer *fb = fb_get();
    if (!fb) return;
    int sw = (int)fb->width;
    int sh = (int)fb->height;

    /* Background */
    draw_rect(0, 0, sw, sh - TASKBAR_H, draw_rgb(0x1A,0x1A,0x2E));

    /* Subtle grid pattern */
    uint32_t grid_col = draw_rgb(0x20,0x20,0x3E);
    for (int x = 0; x < sw; x += 32)
        draw_vline(x, 0, sh - TASKBAR_H, grid_col);
    for (int y = 0; y < sh - TASKBAR_H; y += 32)
        draw_hline(0, y, sw, grid_col);
}

/* Full repaint (called after every state change) */
static void wm_repaint(void)
{
    draw_cursor_hide();
    render_desktop();
    for (int i = 0; i < win_count; i++) {
        if (windows[i].flags & WIN_FLAG_VISIBLE)
            render_window(&windows[i]);
    }
    render_taskbar();

    mouse_state_t ms = mouse_get();
    draw_cursor_show(ms.x, ms.y);
}

/* ══════════════════════════════════════════════════════════════
 *  Window management
 * ══════════════════════════════════════════════════════════════ */

static wm_window_t *wm_create(const wm_create_req_t *req)
{
    if (win_count >= WM_MAX_WINDOWS) return NULL;

    wm_window_t *w = &windows[win_count++];
    w->id     = next_id++;
    w->x      = req->x;
    w->y      = req->y;
    w->width  = req->width  > 0 ? req->width  : 320;
    w->height = req->height > 0 ? req->height : 240;
    w->flags  = WIN_FLAG_VISIBLE;
    w->back_buf = NULL;

    /* copy title */
    int i;
    for (i = 0; i < WM_TITLE_MAX - 1 && req->title[i]; i++)
        w->title[i] = req->title[i];
    w->title[i] = '\0';

    /* connect to app's event port */
    w->event_port = port_find(req->reply_port);

    wm_set_focus(w);
    return w;
}

/* Raise window w to top of z-order */
static void wm_raise(wm_window_t *w)
{
    int idx = -1;
    for (int i = 0; i < win_count; i++)
        if (&windows[i] == w) { idx = i; break; }
    if (idx < 0 || idx == win_count - 1) return;

    /* Shift everything above it down by one */
    wm_window_t tmp = windows[idx];
    for (int i = idx; i < win_count - 1; i++)
        windows[i] = windows[i + 1];
    windows[win_count - 1] = tmp;
    focused_idx = win_count - 1;
}

static void wm_close(wm_window_t *w)
{
    /* Send close event to app */
    if (w->event_port)
        port_send(w->event_port, WM_EVENT_CLOSE, NULL, 0);

    if (w->back_buf) kfree(w->back_buf);

    /* Remove from table */
    int idx = (int)(w - windows);
    for (int i = idx; i < win_count - 1; i++)
        windows[i] = windows[i + 1];
    win_count--;

    /* Update focus to new top window */
    focused_idx = win_count - 1;
    if (focused_idx >= 0) {
        windows[focused_idx].flags |= WIN_FLAG_FOCUSED;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Hit testing
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    HIT_NONE,
    HIT_TITLEBAR,
    HIT_CLOSE_BTN,
    HIT_CLIENT,
} hit_t;

static hit_t hit_test(wm_window_t *w, int mx, int my,
                       int *client_x_out, int *client_y_out)
{
    int dx = decor_x(w);
    int dy = decor_y(w);
    int fw = full_w(w);

    /* Close button */
    int btn_size = 14;
    int btn_x = dx + fw - btn_size - 5;
    int btn_y = dy + (WM_TITLEBAR_H - btn_size) / 2;
    if (mx >= btn_x && mx < btn_x + btn_size &&
        my >= btn_y && my < btn_y + btn_size)
        return HIT_CLOSE_BTN;

    /* Titlebar */
    if (mx >= dx && mx < dx + fw &&
        my >= dy && my < dy + WM_TITLEBAR_H)
        return HIT_TITLEBAR;

    /* Client area */
    if (mx >= w->x && mx < w->x + w->width &&
        my >= w->y && my < w->y + w->height) {
        if (client_x_out) *client_x_out = mx - w->x;
        if (client_y_out) *client_y_out = my - w->y;
        return HIT_CLIENT;
    }

    return HIT_NONE;
}

/* Return topmost window at (mx, my), or NULL. */
static wm_window_t *window_at(int mx, int my,
                               hit_t *ht_out,
                               int *cx_out, int *cy_out)
{
    for (int i = win_count - 1; i >= 0; i--) {
        wm_window_t *w = &windows[i];
        if (!(w->flags & WIN_FLAG_VISIBLE)) continue;
        int cx = 0, cy = 0;
        hit_t ht = hit_test(w, mx, my, &cx, &cy);
        if (ht != HIT_NONE) {
            if (ht_out)  *ht_out = ht;
            if (cx_out)  *cx_out = cx;
            if (cy_out)  *cy_out = cy;
            return w;
        }
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════
 *  IPC message dispatch
 * ══════════════════════════════════════════════════════════════ */

static port_t *wm_port = NULL;

static void handle_message(ipc_message_t *msg)
{
    switch (msg->code) {

    case WM_CREATE_WINDOW: {
        if (!msg->data || msg->length < sizeof(wm_create_req_t)) break;
        wm_create_req_t *req = (wm_create_req_t *)msg->data;
        wm_window_t *w = wm_create(req);
        if (!w) break;

        /* Reply with the assigned window ID */
        if (w->event_port) {
            wm_create_reply_t reply = { .win_id = w->id };
            port_send(w->event_port, WM_REPLY_CREATE,
                      &reply, sizeof(reply));
        }
        wm_repaint();
        break;
    }

    case WM_DESTROY_WINDOW: {
        if (!msg->data || msg->length < sizeof(wm_win_req_t)) break;
        wm_win_req_t *req = (wm_win_req_t *)msg->data;
        wm_window_t  *w   = wm_find(req->win_id);
        if (w) { wm_close(w); wm_repaint(); }
        break;
    }

    case WM_DRAW_RECT: {
        if (!msg->data || msg->length < sizeof(wm_draw_rect_t)) break;
        wm_draw_rect_t *req = (wm_draw_rect_t *)msg->data;
        wm_window_t    *w   = wm_find(req->win_id);
        if (!w) break;

        /* Clip to client area */
        int ax = w->x + req->x;
        int ay = w->y + req->y;
        int aw = req->width;
        int ah = req->height;
        if (ax < w->x) { aw -= w->x - ax; ax = w->x; }
        if (ay < w->y) { ah -= w->y - ay; ay = w->y; }
        if (ax + aw > w->x + w->width)  aw = w->x + w->width  - ax;
        if (ay + ah > w->y + w->height) ah = w->y + w->height - ay;
        if (aw > 0 && ah > 0) {
            uint8_t r = (req->colour >> 16) & 0xFF;
            uint8_t g = (req->colour >>  8) & 0xFF;
            uint8_t b = (req->colour >>  0) & 0xFF;
            draw_rect(ax, ay, aw, ah, draw_rgb(r, g, b));
        }
        /* Move cursor out of the way before partial paint */
        draw_cursor_show(draw_cursor_x(), draw_cursor_y());
        break;
    }

    case WM_DRAW_BITMAP: {
        if (!msg->data || msg->length < sizeof(wm_draw_bitmap_t)) break;
        wm_draw_bitmap_t *req = (wm_draw_bitmap_t *)msg->data;
        wm_window_t      *w   = wm_find(req->win_id);
        if (!w || !req->pixels) break;
        draw_cursor_hide();
        draw_blit(w->x + req->x, w->y + req->y,
                  req->width, req->height,
                  req->pixels, req->width);
        draw_cursor_show(draw_cursor_x(), draw_cursor_y());
        break;
    }

    case WM_SET_TITLE: {
        if (!msg->data || msg->length < sizeof(wm_set_title_t)) break;
        wm_set_title_t *req = (wm_set_title_t *)msg->data;
        wm_window_t    *w   = wm_find(req->win_id);
        if (!w) break;
        int i;
        for (i = 0; i < WM_TITLE_MAX - 1 && req->title[i]; i++)
            w->title[i] = req->title[i];
        w->title[i] = '\0';
        draw_cursor_hide();
        render_window(w);
        draw_cursor_show(draw_cursor_x(), draw_cursor_y());
        break;
    }

    case WM_MOVE_WINDOW: {
        if (!msg->data || msg->length < sizeof(wm_move_t)) break;
        wm_move_t   *req = (wm_move_t *)msg->data;
        wm_window_t *w   = wm_find(req->win_id);
        if (!w) break;
        w->x = req->x;
        w->y = req->y;
        wm_repaint();
        break;
    }

    case WM_RAISE_WINDOW: {
        if (!msg->data || msg->length < sizeof(wm_win_req_t)) break;
        wm_win_req_t *req = (wm_win_req_t *)msg->data;
        wm_window_t  *w   = wm_find(req->win_id);
        if (w) { wm_raise(w); wm_set_focus(w); wm_repaint(); }
        break;
    }

    default:
        break;
    }

    /* Free heap-allocated payload (port_send caller allocates) */
    if (msg->data) kfree(msg->data);
}

/* ══════════════════════════════════════════════════════════════
 *  Mouse event processing
 * ══════════════════════════════════════════════════════════════ */

/* Drag state */
static wm_window_t *drag_win   = NULL;
static int          drag_off_x = 0;
static int          drag_off_y = 0;
static int          last_left  = 0;

static void process_mouse(void)
{
    if (!mouse_moved() && !mouse_get().left && !last_left) return;

    mouse_state_t ms = mouse_get();
    int mx = ms.x;
    int my = ms.y;
    int clicked = ms.left && !last_left;  /* rising edge */
    int released = !ms.left && last_left; /* falling edge */
    last_left = ms.left;

    /* Move cursor */
    draw_cursor_hide();
    draw_cursor_show(mx, my);

    /* Drag in progress */
    if (drag_win && ms.left) {
        drag_win->x = mx - drag_off_x;
        drag_win->y = my - drag_off_y;
        wm_repaint();
        return;
    }
    if (released) {
        drag_win = NULL;
        return;
    }

    if (!clicked) return;

    /* Hit test topmost window */
    hit_t ht;
    int cx, cy;
    wm_window_t *w = window_at(mx, my, &ht, &cx, &cy);

    if (!w) return;

    /* Raise and focus on any click */
    if (!(w->flags & WIN_FLAG_FOCUSED)) {
        wm_raise(w);
        wm_set_focus(w);
        wm_repaint();
    }

    switch (ht) {
    case HIT_CLOSE_BTN:
        wm_close(w);
        wm_repaint();
        break;

    case HIT_TITLEBAR:
        drag_win   = w;
        drag_off_x = mx - w->x;
        drag_off_y = my - w->y;
        break;

    case HIT_CLIENT: {
        /* Forward mouse event to application */
        if (w->event_port) {
            wm_mouse_event_t *ev = kmalloc(sizeof(wm_mouse_event_t));
            if (ev) {
                ev->x      = cx;
                ev->y      = cy;
                ev->dx     = ms.dx;
                ev->dy     = ms.dy;
                ev->left   = ms.left;
                ev->right  = ms.right;
                ev->middle = ms.middle;
                port_send(w->event_port, WM_EVENT_MOUSE,
                          ev, sizeof(*ev));
                /* port_send copies; we own the buffer */
                kfree(ev);
            }
        }
        break;
    }

    default: break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Keyboard routing — send to focused window
 * ══════════════════════════════════════════════════════════════ */

static void process_keyboard(void)
{
    /* keyboard driver already queues scancodes via IRQ;
     * this is a placeholder until you expose a peek/dequeue API.
     * For now, wiring keyboard events through IPC is straightforward
     * once you add keyboard_dequeue() to keyboard.h. */
    (void)0;
}

/* ══════════════════════════════════════════════════════════════
 *  Compositor thread entry point
 * ══════════════════════════════════════════════════════════════ */

void wm_init(void)
{
    /* Zero out window table */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].id = 0;
        windows[i].flags = 0;
        windows[i].back_buf = NULL;
        windows[i].event_port = NULL;
    }
    win_count   = 0;
    focused_idx = -1;

    /* Create the well-known IPC port apps connect to */
    wm_port = port_create(WM_PORT_NAME);
    if (!wm_port)
        kprintf("wm: failed to create port '%s'\n", WM_PORT_NAME);
    else
        kprintf("wm: listening on port '%s'\n", WM_PORT_NAME);

    /* Enable mouse */
    struct limine_framebuffer *fb = fb_get();
    if (fb)
        mouse_set_screen((int)fb->width, (int)fb->height);

    /* Initial repaint */
    wm_repaint();
}

void wm_thread_entry(void)
{
    wm_init();

    kprintf("wm: compositor running\n");

    for (;;) {
        /* Handle all pending IPC messages (non-blocking drain) */
        if (wm_port) {
            ipc_message_t msg;
            while (wm_port->count > 0) {
                if (port_receive(wm_port, &msg) == 0)
                    handle_message(&msg);
            }
        }

        /* Process input */
        process_mouse();
        process_keyboard();

        /* Yield so other threads can run */
        scheduler_yield();
    }
}
