#include "apps.h"
#include "wm.h"
#include "../gfx/gfx.h"
#include "../arch/x86_64/pit.h"
#include "../lib/printf.h"
#include <stdint.h>

static int slen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

// ─── Terminal ─────────────────────────────────────────────────────────────────

static char term_lines[20][80];
static int term_row = 0, term_col = 0;

static void term_print(const char *s)
{
    while (*s)
    {
        if (*s == '\n' || term_col >= 79)
        {
            term_col = 0;
            term_row++;
            if (term_row >= 20)
            {
                for (int i = 0; i < 19; i++)
                    for (int j = 0; j < 80; j++)
                        term_lines[i][j] = term_lines[i + 1][j];
                for (int j = 0; j < 80; j++)
                    term_lines[19][j] = 0;
                term_row = 19;
            }
            if (*s == '\n')
            {
                s++;
                continue;
            }
        }
        term_lines[term_row][term_col++] = *s++;
    }
}

static void draw_terminal(int id, int x, int y, int w, int h)
{
    (void)id;
    uint32_t bg = RGB(0x0d, 0x0d, 0x0d), fg = RGB(0x00, 0xff, 0x88);
    gfx_fill_rect(x, y, w, h, bg);
    for (int row = 0; row < 20 && row * 14 < h; row++)
    {
        for (int col = 0; col < 79; col++)
        {
            char c = term_lines[row][col];
            if (!c)
                break;
            uint32_t colour = fg;
            if (c == 'r' && col == 0)
                colour = RGB(0x48, 0xda, 0xff);
            gfx_char(x + col * 8, y + row * 14, c, colour, bg);
        }
    }
    if ((pit_ticks() / 50) % 2 == 0)
        gfx_fill_rect(x + term_col * 8, y + term_row * 14 + 1, 8, 11, fg);
}

// ─── About ────────────────────────────────────────────────────────────────────

static void draw_about(int id, int x, int y, int w, int h)
{
    (void)id;
    uint32_t bg = RGB(0x1c, 0x1c, 0x1e);
    gfx_fill_rect(x, y, w, h, bg);

    int cx = x + w / 2, cy = y + 60;
    gfx_fill_circle(cx, cy, 40, RGB(0x48, 0xda, 0xff));
    gfx_fill_circle(cx, cy, 34, bg);
    gfx_text(cx - 8, cy - 4, "OS", RGB(0x48, 0xda, 0xff), bg);

    gfx_text(cx - 28, cy + 52, "TermuOS", RGB(0xff, 0xff, 0xff), bg);
    gfx_text(cx - 20, cy + 68, "v0.1.0", RGB(0x8e, 0x8e, 0x93), bg);
    gfx_text(cx - 56, cy + 86, "x86_64 Hobby Kernel", RGB(0x8e, 0x8e, 0x93), bg);
    gfx_hline(x + 20, cy + 106, w - 40, RGB(0x3a, 0x3a, 0x3c));
    gfx_text(x + 20, cy + 116, "Kernel:   TermuOS 0.1.0", RGB(0xff, 0xff, 0xff), bg);
    gfx_text(x + 20, cy + 132, "Arch:     x86_64", RGB(0xff, 0xff, 0xff), bg);
    gfx_text(x + 20, cy + 148, "WM:       TermuWM 1.0", RGB(0xff, 0xff, 0xff), bg);
    gfx_text(x + 20, cy + 164, "GFX:      Framebuffer", RGB(0xff, 0xff, 0xff), bg);
}

// ─── Files ────────────────────────────────────────────────────────────────────

static const char *flist[] = {"bin/", "etc/", "home/", "motd", "kernel.elf", NULL};
static int fsel = 0;

static void draw_files(int id, int x, int y, int w, int h)
{
    (void)id;
    uint32_t bg = RGB(0x1c, 0x1c, 0x1e);
    uint32_t sbg = RGB(0x30, 0x6e, 0xff);
    uint32_t sbg2 = RGB(0x16, 0x16, 0x18);

    // Sidebar
    gfx_fill_rect(x, y, 120, h, sbg2);
    gfx_vline(x + 120, y, h, RGB(0x3a, 0x3a, 0x3c));
    const char *places[] = {"/ Root", "~ Home", "etc", "bin", NULL};
    for (int i = 0; places[i]; i++)
    {
        uint32_t pb = (i == 0) ? RGB(0x2a, 0x2a, 0x4a) : sbg2;
        gfx_fill_rect(x, y + i * 28, 120, 28, pb);
        gfx_text(x + 12, y + i * 28 + 10, places[i], RGB(0xcc, 0xcc, 0xcc), pb);
    }

    // Main area
    gfx_fill_rect(x + 121, y, w - 121, h, bg);
    uint32_t hdr = RGB(0x22, 0x22, 0x24);
    gfx_fill_rect(x + 121, y, w - 121, 24, hdr);
    gfx_text(x + 130, y + 8, "Name", RGB(0x8e, 0x8e, 0x93), hdr);
    gfx_text(x + 300, y + 8, "Type", RGB(0x8e, 0x8e, 0x93), hdr);
    gfx_hline(x + 121, y + 24, w - 121, RGB(0x3a, 0x3a, 0x3c));

    for (int i = 0; flist[i]; i++)
    {
        int fy = y + 24 + i * 26;
        if (fy + 26 > y + h)
            break;
        int sel = (i == fsel);
        int isdir = (flist[i][slen(flist[i]) - 1] == '/');
        uint32_t rb = sel ? sbg : (i % 2 ? bg : RGB(0x1e, 0x1e, 0x20));
        gfx_fill_rect(x + 121, fy, w - 121, 26, rb);
        gfx_text(x + 130, fy + 8, isdir ? "D" : "F", isdir ? RGB(0x48, 0xda, 0xff) : RGB(0xcc, 0xcc, 0xcc), rb);
        gfx_text(x + 146, fy + 8, flist[i], RGB(0xff, 0xff, 0xff), rb);
        gfx_text(x + 300, fy + 8, isdir ? "Folder" : "File", RGB(0x8e, 0x8e, 0x93), rb);
    }
}

// ─── Clock ────────────────────────────────────────────────────────────────────

static const int COS[60] = {
    100, 99, 98, 95, 92, 87, 81, 74, 67, 59, 50, 41, 31, 21, 10,
    0, -10, -21, -31, -41, -50, -59, -67, -74, -81, -87, -92, -95, -98, -99,
    -100, -99, -98, -95, -92, -87, -81, -74, -67, -59, -50, -41, -31, -21, -10,
    0, 10, 21, 31, 41, 50, 59, 67, 74, 81, 87, 92, 95, 98, 99};
static const int SIN[60] = {
    0, 10, 21, 31, 41, 50, 59, 67, 74, 81, 87, 92, 95, 98, 99,
    100, 99, 98, 95, 92, 87, 81, 74, 67, 59, 50, 41, 31, 21, 10,
    0, -10, -21, -31, -41, -50, -59, -67, -74, -81, -87, -92, -95, -98, -99,
    -100, -99, -98, -95, -92, -87, -81, -74, -67, -59, -50, -41, -31, -21, -10};

static void draw_clock(int id, int x, int y, int w, int h)
{
    (void)id;
    uint32_t bg = RGB(0x1c, 0x1c, 0x1e);
    gfx_fill_rect(x, y, w, h, bg);

    int cx = x + w / 2, cy = y + h / 2 - 10;
    int r = (w < h ? w : h) / 2 - 16;

    gfx_fill_circle(cx, cy, r, RGB(0x16, 0x16, 0x18));
    gfx_circle(cx, cy, r, RGB(0x48, 0xda, 0xff));
    gfx_circle(cx, cy, r - 2, RGB(0x3a, 0x3a, 0x3c));

    // Hour markers
    for (int i = 0; i < 12; i++)
    {
        int mi = i * 5;
        int hx = cx + COS[mi] * (r - 8) / 100;
        int hy = cy - SIN[mi] * (r - 8) / 100;
        gfx_fill_circle(hx, hy, 2, RGB(0x8e, 0x8e, 0x93));
    }

    uint64_t t = pit_ticks(), s = t / 100;
    uint64_t sm = s % 60, mm = (s / 60) % 60, hh = (s / 3600) % 12;

    // Hour hand
    int ha = (int)(hh * 5 + mm / 12) % 60;
    gfx_line(cx, cy, cx + SIN[ha] * (r - 24) / 100, cy - COS[ha] * (r - 24) / 100,
             RGB(0xff, 0xff, 0xff));
    // Minute hand
    gfx_line(cx, cy, cx + SIN[mm] * (r - 12) / 100, cy - COS[mm] * (r - 12) / 100,
             RGB(0xff, 0xff, 0xff));
    // Second hand
    gfx_line(cx, cy, cx + SIN[sm] * (r - 4) / 100, cy - COS[sm] * (r - 4) / 100,
             RGB(0xff, 0x5f, 0x57));

    gfx_fill_circle(cx, cy, 4, RGB(0x48, 0xda, 0xff));

    // Digital
    char clk[9];
    uint64_t dh = s / 3600 % 24;
    clk[0] = '0' + dh / 10;
    clk[1] = '0' + dh % 10;
    clk[2] = ':';
    clk[3] = '0' + mm / 10;
    clk[4] = '0' + mm % 10;
    clk[5] = ':';
    clk[6] = '0' + sm / 10;
    clk[7] = '0' + sm % 10;
    clk[8] = 0;
    gfx_text(cx - 32, cy + r + 10, clk, RGB(0x48, 0xda, 0xff), bg);
}

// ─── Init ─────────────────────────────────────────────────────────────────────

void apps_init(void)
{
    int W = gfx_width(), H = gfx_height();

    for (int i = 0; i < 20; i++)
        for (int j = 0; j < 80; j++)
            term_lines[i][j] = 0;
    term_print("root@TermuOS:~# uname -a\n");
    term_print("TermuOS 0.1.0 x86_64\n");
    term_print("root@TermuOS:~# mem\n");
    term_print("Total:128MB  Used:4MB  Free:124MB\n");
    term_print("root@TermuOS:~# ls /\n");
    term_print("bin/  etc/  home/\n");
    term_print("root@TermuOS:~# ");

    // Windows positioned below 32px menubar
    wm_create("Terminal", W / 2 - 260, 50, 520, 300, WM_FLAG_RESIZABLE, draw_terminal);
    wm_create("Files", 30, 100, 480, 300, WM_FLAG_RESIZABLE, draw_files);
    wm_create("Clock", W - 230, 50, 200, 220, 0, draw_clock);
    wm_create("About TermuOS", W / 2 - 150, H / 2 - 180, 300, 360, 0, draw_about);
}