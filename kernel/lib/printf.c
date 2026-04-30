#include "printf.h"
#include "../drivers/video/terminal.h"
#include <stdarg.h>
#include <stdint.h>

static void print_uint(uint64_t n, int base)
{
    static const char digits[] = "0123456789abcdef";
    char buf[64];
    int i = 0;
    if (n == 0)
    {
        terminal_putchar('0');
        return;
    }
    while (n)
    {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i--)
        terminal_putchar(buf[i]);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    for (; *fmt; fmt++)
    {
        if (*fmt != '%')
        {
            terminal_putchar(*fmt);
            continue;
        }
        fmt++;
        switch (*fmt)
        {
        case 'c':
            terminal_putchar((char)va_arg(args, int));
            break;
        case 's':
            terminal_puts(va_arg(args, const char *));
            break;
        case 'd':
        {
            int64_t n = va_arg(args, int64_t);
            if (n < 0)
            {
                terminal_putchar('-');
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
            terminal_puts("0x");
            print_uint((uint64_t)va_arg(args, void *), 16);
            break;
        case '%':
            terminal_putchar('%');
            break;
        default:
            terminal_putchar('?');
            break;
        }
    }

    va_end(args);
}