#include "keyboard.h"
#include "../../arch/x86_64/idt.h"
#include "../../lib/printf.h"
#include <stdint.h>

#define KBD_DATA    0x60
#define KBD_STATUS  0x64

static const char sc_normal[128] = {
    0,   0,  '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`',  0,'\\','z','x','c','v',
    'b','n','m',',','.','/',  0, '*',  0, ' ',  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0, '7','8','9','-','4','5','6','+','1',
    '2','3','0','.',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const char sc_shifted[128] = {
    0,   0,  '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"', '~',  0, '|','Z','X','C','V',
    'B','N','M','<','>','?',  0, '*',  0, ' ',  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0, '7','8','9','-','4','5','6','+','1',
    '2','3','0','.',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static int shift = 0;
static int caps  = 0;

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}

void keyboard_init(void)
{
    while (inb(KBD_STATUS) & 0x01)
        inb(KBD_DATA);
    kprintf("Keyboard: ready.\n");
}

int keyboard_haschar(void) { return inb(KBD_STATUS) & 0x01; }

char keyboard_getchar(void)
{
    while (1) {
        while (!(inb(KBD_STATUS) & 0x01))
            ;

        uint8_t sc = inb(KBD_DATA);

        if (sc & 0x80) {
            uint8_t rel = sc & 0x7f;
            if (rel == 0x2a || rel == 0x36) shift = 0;
            continue;
        }

        if (sc == 0x2a || sc == 0x36) { shift = 1; continue; }
        if (sc == 0x3a) { caps = !caps; continue; }
        if (sc == 0x1d || sc == 0x38) continue;
        if (sc >= 128) continue;

        int upper = shift ^ caps;
        char c = upper ? sc_shifted[sc] : sc_normal[sc];
        if (c) return c;
    }
}