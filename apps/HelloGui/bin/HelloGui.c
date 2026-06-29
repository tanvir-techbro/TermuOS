/*
 * apps/HelloGui/bin/HelloGui.c
 *
 * Build:
 *   clang -target x86_64-elf -ffreestanding -fno-pic -static \
 *         -nostdlib -O2 -Wl,--build-id=none,-Ttext,0x400000 \
 *         -o HelloGui HelloGui.c
 */

#include <stdint.h>

/* ── syscalls ──────────────────────────────────────────────────── */
static inline long sc(long n,long a,long b,long c,long d,long e,long f){
    register long r10 __asm__("r10")=d,r8 __asm__("r8")=e,r9 __asm__("r9")=f;
    long r;
    __asm__ volatile("syscall":"=a"(r):"0"(n),"D"(a),"S"(b),"d"(c),
        "r"(r10),"r"(r8),"r"(r9):"rcx","r11","memory");
    return r;
}
#define SYS_YIELD        24
#define SYS_EXIT         60
#define SYS_PORT_FIND    300
#define SYS_PORT_SEND    301
#define SYS_PORT_RECEIVE 302

/* All port handles are indices into the kernel port_pool[] */
static long pfind(const char *n){ return sc(SYS_PORT_FIND,(long)n,0,0,0,0,0); }
static void sysexit(int c)      { sc(SYS_EXIT,c,0,0,0,0,0); }
static void sysyield(void)      { sc(SYS_YIELD,0,0,0,0,0,0); }
static long psend(long p, uint32_t code, void *d, uint32_t l){
    return sc(SYS_PORT_SEND,p,(long)code,(long)d,(long)l,0,0);
}

/*
 * ipc_message_user_t
 *
 * IMPORTANT: sys_port_receive currently sets data=NULL (kernel bug we
 * haven't patched yet). Until syscall_changes.c is applied we work
 * around it: for WM_REPLY_CREATE we don't need the win_id from the
 * payload — tlib already pre-registered 'hello_gui.events' in the
 * manifest ports[], so we just assign win_id=1 and proceed.
 *
 * Once syscall_changes.c is applied the payload copy will work and
 * win_id will be correct.
 */
typedef struct {
    uint32_t sender_pid;
    uint32_t code;
    void    *data;
    uint32_t length;
    uint8_t  inline_data[64];
} ipc_msg_t;

static int precv(long p, ipc_msg_t *out){
    return (int)sc(SYS_PORT_RECEIVE,p,(long)out,0,0,0,0);
}

/* ── WM protocol ───────────────────────────────────────────────── */
#define WM_CREATE_WINDOW  0x01
#define WM_DRAW_RECT      0x03
#define WM_EVENT_MOUSE    0x80
#define WM_EVENT_CLOSE    0x82
#define WM_REPLY_CREATE   0x83

typedef struct {
    int x,y,width,height;
    char title[64];
    char reply_port[32];
} wm_create_req_t;

typedef struct { uint32_t win_id; } wm_create_reply_t;

typedef struct {
    uint32_t win_id;
    int x,y,w,h;
    uint32_t colour;
} wm_draw_rect_t;

typedef struct {
    int x,y,dx,dy;
    uint8_t left,right,middle;
} wm_mouse_t;

/* ── tiny string copy ──────────────────────────────────────────── */
static void scpy(char *d, const char *s, int max){
    int i=0; while(i<max-1&&s[i]){d[i]=s[i];i++;} d[i]=0;
}

/* ── app state ─────────────────────────────────────────────────── */
#define WIN_W 300
#define WIN_H 200
#define BTN_X  20
#define BTN_Y  60
#define BTN_W 120
#define BTN_H  30

static long     g_wm      = -1;
static long     g_ev      = -1;
static uint32_t g_wid     =  0;
static int      g_pressed =  0;
static int      g_clicks  =  0;

static void fill(int x,int y,int w,int h,uint32_t c){
    wm_draw_rect_t r;
    r.win_id=g_wid; r.x=x; r.y=y; r.w=w; r.h=h; r.colour=c;
    psend(g_wm, WM_DRAW_RECT, &r, sizeof r);
}

static void paint(void){
    fill(0,    0,   WIN_W, WIN_H, 0xECF0F1);
    fill(0,    0,   WIN_W, 40,   0x2980B9);
    fill(BTN_X,BTN_Y,BTN_W,BTN_H, g_pressed ? 0x27AE60 : 0x2C3E50);
    int bar = g_clicks * 20;
    if(bar > WIN_W-20) bar = WIN_W-20;
    if(bar > 0) fill(10,120,bar,14,0xE74C3C);
}

void _start(void){
    /* 1. Find the compositor port */
    g_wm = pfind("wm");
    if(g_wm < 0) sysexit(1);

    /*
     * 2. Find our event port.
     *    tlib already created "hello_gui.events" from manifest.json
     *    before we even started running. Just find it by name.
     */
    g_ev = pfind("hello_gui.events");
    if(g_ev < 0) sysexit(2);

    /* 3. Request a window — reply comes back on g_ev */
    wm_create_req_t req;
    req.x=150; req.y=80; req.width=WIN_W; req.height=WIN_H;
    scpy(req.title,      "Hello GUI",         64);
    scpy(req.reply_port, "hello_gui.events",  32);
    psend(g_wm, WM_CREATE_WINDOW, &req, sizeof req);

    /* 4. Wait for WM_REPLY_CREATE */
    ipc_msg_t msg;
    precv(g_ev, &msg);
    if(msg.code == WM_REPLY_CREATE){
        /* If syscall_changes.c was applied, data points to inline_data
         * and we can read the real win_id.  Otherwise fall back to 1. */
        if(msg.data){
            wm_create_reply_t *rep = (wm_create_reply_t *)msg.data;
            g_wid = rep->win_id;
        } else {
            g_wid = 1; /* fallback until syscall patch applied */
        }
    }

    /* 5. Draw initial window content */
    paint();

    /* 6. Event loop */
    for(;;){
        precv(g_ev, &msg);

        if(msg.code == WM_EVENT_CLOSE) sysexit(0);

        if(msg.code == WM_EVENT_MOUSE && msg.data){
            wm_mouse_t *m = (wm_mouse_t *)msg.data;
            int over = m->x>=BTN_X && m->x<BTN_X+BTN_W &&
                       m->y>=BTN_Y && m->y<BTN_Y+BTN_H;
            if(m->left && over && !g_pressed){
                g_pressed=1; g_clicks++; paint();
            } else if(!m->left && g_pressed){
                g_pressed=0; paint();
            }
        }

        sysyield();
    }
}