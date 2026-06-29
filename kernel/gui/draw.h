#pragma once
#include <stdint.h>

/*
 * draw.h — low-level 2-D rendering on top of fb_putpixel.
 *
 * All coordinates are in screen space (absolute pixels).
 * Clipping is against the full framebuffer; no window clipping here —
 * the compositor clips before calling these routines.
 */

/* ─── colour helpers ─────────────────────────────────────────── */
/* Pack an RGB triplet into the framebuffer's native 32-bit word.  */
uint32_t draw_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Lighten / darken a packed colour by ±amount (0-255). */
uint32_t draw_lighten(uint32_t colour, uint8_t amount);
uint32_t draw_darken (uint32_t colour, uint8_t amount);

/* ─── primitives ─────────────────────────────────────────────── */
void draw_rect      (int x, int y, int w, int h, uint32_t colour);
void draw_rect_outline(int x, int y, int w, int h, int thickness,
                       uint32_t colour);
void draw_hline     (int x, int y, int len, uint32_t colour);
void draw_vline     (int x, int y, int len, uint32_t colour);

/* ─── bitmap blit ────────────────────────────────────────────── */
/*
 * Blit a w×h pixel buffer (row-major, 32-bit packed colour matching
 * fb_colour() format) to screen position (x, y).
 * src_stride = number of uint32_t per row in the source buffer
 * (usually == w, but can be wider for sub-rect blits).
 */
void draw_blit      (int x, int y, int w, int h,
                     const uint32_t *src, int src_stride);

/* ─── text ───────────────────────────────────────────────────── */
/*
 * Tiny built-in 8×8 font (PC BIOS charset).
 * draw_char  — one glyph at (x, y).
 * draw_text  — null-terminated string, left-to-right, no wrapping.
 * draw_textf — like draw_text but clips to [cx, cx+cw).
 */
#define FONT_W 8
#define FONT_H 8

void draw_char  (int x, int y, char c,
                 uint32_t fg, uint32_t bg, int transparent_bg);
void draw_text  (int x, int y, const char *s,
                 uint32_t fg, uint32_t bg, int transparent_bg);
void draw_textf (int x, int y, int clip_x, int clip_w,
                 const char *s, uint32_t fg, uint32_t bg,
                 int transparent_bg);

/* ─── software cursor ────────────────────────────────────────── */
#define CURSOR_W 11
#define CURSOR_H 18

/*
 * Save the pixels under the cursor, draw the sprite.
 * Call draw_cursor_hide() before moving, then draw_cursor_show() again.
 */
void draw_cursor_show(int x, int y);
void draw_cursor_hide(void);          /* restores saved pixels     */
int  draw_cursor_x(void);
int  draw_cursor_y(void);
