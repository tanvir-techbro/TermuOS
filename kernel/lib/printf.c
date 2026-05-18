#include "printf.h"
#include "../drivers/video/terminal.h"
#include <stdarg.h>
#include <stdint.h>

static void (*_putchar_fn)(char) = terminal_putchar;

static void null_putchar(char c) { (void)c; }

void kprintf_set_output(void (*putchar_fn)(char))
{
    _putchar_fn = putchar_fn ? putchar_fn : null_putchar;
}

static void print_uint(uint64_t n, int base)
{
    static const char digits[] = "0123456789abcdef";
    char buf[64];
    int i = 0;
    if (n == 0)
    {
        _putchar_fn('0');
        return;
    }
    while (n)
    {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i--)
        _putchar_fn(buf[i]);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    const char *s;
    va_start(args, fmt);

    for (; *fmt; fmt++)
    {
        if (*fmt != '%')
        {
            _putchar_fn(*fmt);
            continue;
        }
        fmt++;
        switch (*fmt)
        {
        case 'c':
            _putchar_fn((char)va_arg(args, int));
            break;
        case 's':
            s = va_arg(args, const char *);
            while (s && *s) _putchar_fn(*s++);
            break;
        case 'd':
        {
            int64_t n = va_arg(args, int64_t);
            if (n < 0)
            {
                _putchar_fn('-');
                n = -n;
            }
            print_uint((uint64_t)n, 10);
            break;
        }
        case 'u':
            print_uint(va_arg(args, uint64_t), 10);
            break;
        case 'x':
            print_uint(va_arg(args, uint64_t), 16);
            break;
        case 'p':
            _putchar_fn('0'); _putchar_fn('x');
            print_uint((uint64_t)va_arg(args, void *), 16);
            break;
        case '%':
            _putchar_fn('%');
            break;
        default:
            _putchar_fn('?');
            break;
        }
    }

    va_end(args);
}
