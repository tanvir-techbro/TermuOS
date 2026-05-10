#include "shell.h"
#include "../drivers/video/terminal.h"
#include "../drivers/input/keyboard.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../arch/x86_64/pit.h"
#include "../sched/scheduler.h"
#include "../fs/vfs.h"
#include "../net/net.h"
#include "../desktop/desktop.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define INPUT_MAX 256
#define MAX_ARGS 16
#define HOSTNAME "TermuOS"
#define USERNAME "root"

static char cwd[VFS_PATH_MAX] = "/";

static shell_output_fn shell_out = NULL;

static void shell_emit_char(char c)
{
    if (shell_out)
        shell_out(c);
    else
        terminal_putchar(c);
}

static void shell_print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt)
    {
        if (*fmt != '%')
        {
            shell_emit_char(*fmt++);
            continue;
        }

        fmt++;

        int left_align = 0;
        int zero_pad = 0;
        int width = 0;

        // flags
        if (*fmt == '-')
        {
            left_align = 1;
            fmt++;
        }

        if (*fmt == '0')
        {
            zero_pad = 1;
            fmt++;
        }

        // width
        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        char buf[64];
        char *str = buf;
        int len = 0;

        switch (*fmt)
        {
        case 'c':
        {
            buf[0] = (char)va_arg(args, int);
            buf[1] = 0;
            len = 1;
            break;
        }

        case 's':
        {
            str = va_arg(args, char *);

            if (!str)
                str = "(null)";

            while (str[len])
                len++;

            break;
        }

        case 'd':
        {
            int n = va_arg(args, int);

            if (n < 0)
            {
                buf[len++] = '-';
                n = -n;
            }

            char tmp[32];
            int ti = 0;

            if (n == 0)
                tmp[ti++] = '0';

            while (n > 0)
            {
                tmp[ti++] = '0' + (n % 10);
                n /= 10;
            }

            while (ti--)
                buf[len++] = tmp[ti];

            buf[len] = 0;
            break;
        }

        case 'u':
        {
            unsigned int n = va_arg(args, unsigned int);

            char tmp[32];
            int ti = 0;

            if (n == 0)
                tmp[ti++] = '0';

            while (n > 0)
            {
                tmp[ti++] = '0' + (n % 10);
                n /= 10;
            }

            while (ti--)
                buf[len++] = tmp[ti];

            buf[len] = 0;
            break;
        }

        case 'x':
        {
            unsigned int n = va_arg(args, unsigned int);

            char hex[] = "0123456789abcdef";
            char tmp[32];
            int ti = 0;

            if (n == 0)
                tmp[ti++] = '0';

            while (n > 0)
            {
                tmp[ti++] = hex[n & 0xF];
                n >>= 4;
            }

            while (ti--)
                buf[len++] = tmp[ti];

            buf[len] = 0;
            break;
        }

        case '%':
        {
            buf[0] = '%';
            buf[1] = 0;
            len = 1;
            break;
        }

        default:
        {
            shell_emit_char('%');
            shell_emit_char(*fmt);
            fmt++;
            continue;
        }
        }

        int pad = width - len;
        if (pad < 0)
            pad = 0;

        if (!left_align)
        {
            while (pad--)
                shell_emit_char(zero_pad ? '0' : ' ');
        }

        for (int i = 0; i < len; i++)
            shell_emit_char(str[i]);

        if (left_align)
        {
            while (pad--)
                shell_emit_char(' ');
        }

        fmt++;
    }

    va_end(args);
}

static int sh_strlen(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}
static int sh_strcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b)
    {
        a++;
        b++;
    }
    return *a - *b;
}
static void sh_strcpy(char *d, const char *s, int m)
{
    int i = 0;
    while (s[i] && i < m - 1)
    {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}
static void sh_memset(void *p, uint8_t v, int n)
{
    uint8_t *b = p;
    while (n--)
        *b++ = v;
}

static void resolve_path(const char *arg, char *out, int max)
{
    if (arg[0] == '/')
    {
        sh_strcpy(out, arg, max);
        return;
    }
    int cl = sh_strlen(cwd);
    sh_strcpy(out, cwd, max);
    if (cwd[cl - 1] != '/')
    {
        out[cl] = '/';
        out[cl + 1] = 0;
    }
    int ol = sh_strlen(out);
    int i = 0;
    while (arg[i] && ol + i < max - 1)
    {
        out[ol + i] = arg[i];
        i++;
    }
    out[ol + i] = 0;
}

static int readline(char *buf, int max)
{
    int len = 0;

    while (1)
    {
        char c = keyboard_getchar();

        // ignore empty/no-key values
        if (c == 0)
            continue;

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            terminal_putchar('\n');
            buf[len] = 0;
            return len;
        }

        if (c == '\b')
        {
            if (len > 0)
            {
                len--;

                terminal_putchar('\b');
                terminal_putchar(' ');
                terminal_putchar('\b');
            }

            continue;
        }

        if (len < max - 1)
        {
            buf[len++] = c;
            terminal_putchar(c);
        }
    }
}

static int parse_args(char *line, char **argv)
{
    int argc = 0;
    char *p = line;
    while (*p)
    {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        if (argc >= MAX_ARGS)
            break;
        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = 0;
    }
    argv[argc] = NULL;
    return argc;
}

// ─── Commands ─────────────────────────────────────────────────────────────────

static void cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    shell_print("Commands: help clear echo uname uptime mem threads\n");
    shell_print("          ls cd pwd cat write touch mkdir rm reboot\n");
    shell_print("          ifconfig ping arp\n");
    shell_print("          gui\n");
}

static void cmd_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    terminal_set_bg(0x0d, 0x0d, 0x0d);
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        terminal_puts(argv[i]);
        if (i < argc - 1)
            terminal_putchar(' ');
    }
    terminal_putchar('\n');
}

static void cmd_uname(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    shell_print("TermuOS 0.1.0 x86_64\n");
}

static void cmd_uptime(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint64_t t = pit_ticks(), s = t / 100, m = s / 60, h = m / 60;
    shell_print("up %u:%02u:%02u\n", h, m % 60, s % 60);
}

static void cmd_mem(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    size_t f = pmm_free_pages(), t = pmm_total_pages(), u = t - f;
    shell_print("Total:%uMB Used:%uMB Free:%uMB\n",
                (t * 4096) / (1024 * 1024), (u * 4096) / (1024 * 1024), (f * 4096) / (1024 * 1024));
}

static void cmd_threads(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    extern thread_t threads[MAX_THREADS];
    static const char *st[] = {"dead", "ready", "running", "blocked"};
    shell_print("%-4s %-16s %s\n", "ID", "NAME", "STATE");
    for (int i = 0; i < MAX_THREADS; i++)
    {
        if (threads[i].state == THREAD_DEAD)
            continue;
        shell_print("%-4u %-16s %s\n", threads[i].id, threads[i].name, st[threads[i].state]);
    }
}

static void cmd_pwd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    shell_print("%s\n", cwd);
}

static void cmd_cd(int argc, char **argv)
{
    if (argc < 2)
    {
        shell_print("cd: missing argument\n");
        return;
    }
    static char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    uint32_t type;
    if (vfs_stat(path, &type, NULL) < 0)
    {
        shell_print("cd: %s: not found\n", argv[1]);
        return;
    }
    if (type != VFS_DIR)
    {
        shell_print("cd: %s: not a directory\n", argv[1]);
        return;
    }
    sh_strcpy(cwd, path, VFS_PATH_MAX);
}

static void cmd_ls(int argc, char **argv)
{
    static char path[VFS_PATH_MAX];
    if (argc < 2)
        sh_strcpy(path, cwd, VFS_PATH_MAX);
    else
        resolve_path(argv[1], path, VFS_PATH_MAX);
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0)
    {
        shell_print("ls: %s: not found\n", path);
        return;
    }
    char name[VFS_NAME_MAX];
    uint32_t idx = 0;
    while (vfs_readdir(fd, idx++, name) == 0)
    {
        static char full[VFS_PATH_MAX];
        int pl = sh_strlen(path);
        sh_strcpy(full, path, VFS_PATH_MAX);
        if (full[pl - 1] != '/')
        {
            full[pl] = '/';
            full[pl + 1] = 0;
            pl++;
        }
        int ni = 0;
        while (name[ni] && pl + ni < VFS_PATH_MAX - 1)
        {
            full[pl + ni] = name[ni];
            ni++;
        }
        full[pl + ni] = 0;
        uint32_t type = VFS_FILE;
        uint64_t size = 0;
        vfs_stat(full, &type, &size);
        if (type == VFS_DIR)
            shell_print("%s/\n", name);
        else
            shell_print("%s  (%u bytes)\n", name, size);
    }
    vfs_close(fd);
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        shell_print("cat: missing filename\n");
        return;
    }
    static char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0)
    {
        shell_print("cat: %s: not found\n", argv[1]);
        return;
    }
    uint8_t buf[256];
    int n;
    while ((n = vfs_read(fd, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = 0;
        terminal_puts((char *)buf);
    }
    terminal_putchar('\n');
    vfs_close(fd);
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 3)
    {
        shell_print("write: usage: write <file> <text>\n");
        return;
    }
    static char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    int fd = vfs_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        shell_print("write: cannot open %s\n", argv[1]);
        return;
    }
    for (int i = 2; i < argc; i++)
    {
        vfs_write(fd, argv[i], sh_strlen(argv[i]));
        if (i < argc - 1)
            vfs_write(fd, " ", 1);
    }
    vfs_write(fd, "\n", 1);
    vfs_close(fd);
}

static void cmd_touch(int argc, char **argv)
{
    if (argc < 2)
    {
        shell_print("touch: missing filename\n");
        return;
    }
    static char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    if (vfs_create(path) < 0)
        shell_print("touch: failed\n");
}

static void cmd_mkdir(int argc, char **argv)
{
    if (argc < 2)
    {
        shell_print("mkdir: missing dirname\n");
        return;
    }
    static char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    if (vfs_mkdir(path) < 0)
        shell_print("mkdir: failed\n");
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        shell_print("rm: missing filename\n");
        return;
    }
    static char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    if (vfs_unlink(path) < 0)
        shell_print("rm: %s: not found\n", argv[1]);
}

static void cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    shell_print("Rebooting...\n");
    uint8_t v = 0;
    while (v & 0x02)
        __asm__ volatile("inb $0x64,%0" : "=a"(v));
    __asm__ volatile("outb %0,$0x64" ::"a"((uint8_t)0xfe));
    for (;;)
        __asm__ volatile("hlt");
}

// ─── Network commands ─────────────────────────────────────────────────────────

static void cmd_ifconfig(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    shell_print("eth0: MAC " MAC_FMT "\n", MAC_ARGS(netif.mac));
    shell_print("      IP  " IP_FMT "\n", IP_ARGS(netif.ip));
    shell_print("      GW  " IP_FMT "\n", IP_ARGS(netif.gateway));
}

static void cmd_ping(int argc, char **argv)
{
    if (argc < 2)
    {
        shell_print("ping: usage: ping <ip>\n");
        return;
    }
    ip4_t dst = {0};
    const char *s = argv[1];
    int octet = 0, val = 0;
    while (*s)
    {
        if (*s >= '0' && *s <= '9')
            val = val * 10 + (*s - '0');
        else if (*s == '.')
        {
            dst.b[octet++] = val;
            val = 0;
        }
        s++;
    }
    dst.b[octet] = val;
    shell_print("ping: " IP_FMT "\n", IP_ARGS(dst));
    net_send_arp_request(dst);
    for (volatile int i = 0; i < 10000000; i++)
        ;
    net_send_icmp_echo(dst, 1, 1);
}

static void cmd_arp(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    shell_print("arp: requesting gateway " IP_FMT "\n", IP_ARGS(netif.gateway));
    net_send_arp_request(netif.gateway);
}

// ─── GUI ─────────────────────────────────────────────────────────────────
static void cmd_gui(int argc, char **argv)
{
    desktop_run();
}

// ─── Dispatch ─────────────────────────────────────────────────────────────────

typedef struct
{
    const char *name;
    void (*fn)(int, char **);
} command_t;
static const command_t commands[] = {
    {"help", cmd_help}, {"clear", cmd_clear}, {"echo", cmd_echo}, {"uname", cmd_uname}, {"uptime", cmd_uptime}, {"mem", cmd_mem}, {"threads", cmd_threads}, {"pwd", cmd_pwd}, {"cd", cmd_cd}, {"ls", cmd_ls}, {"cat", cmd_cat}, {"write", cmd_write}, {"touch", cmd_touch}, {"mkdir", cmd_mkdir}, {"rm", cmd_rm}, {"reboot", cmd_reboot}, {"ifconfig", cmd_ifconfig}, {"ping", cmd_ping}, {"arp", cmd_arp}, {"gui", cmd_gui}, {NULL, NULL}};

static void dispatch(char *line)
{
    if (!line || !line[0])
        return;
    char *argv[MAX_ARGS];
    int argc = parse_args(line, argv);
    if (!argc)
        return;
    for (int i = 0; commands[i].name; i++)
        if (sh_strcmp(argv[0], commands[i].name) == 0)
        {
            commands[i].fn(argc, argv);
            return;
        }
    shell_print("sh: command not found: %s\n", argv[0]);
}

static void print_prompt(void)
{
    terminal_set_fg(0x00, 0xff, 0x88);
    terminal_puts(USERNAME "@" HOSTNAME);
    terminal_set_fg(0xff, 0xff, 0xff);
    terminal_putchar(':');
    terminal_set_fg(0x55, 0xaa, 0xff);
    terminal_puts(cwd);
    terminal_set_fg(0xff, 0xff, 0xff);
    terminal_puts("# ");
}

void shell_set_output(shell_output_fn fn)
{
    shell_out = fn;
}

void shell_run(void)
{
    char input[INPUT_MAX];
    shell_print("\nTermuOS 0.1.0 -- type 'help' for commands.\n\n");
    while (1)
    {
        print_prompt();
        sh_memset(input, 0, sizeof(input));
        readline(input, INPUT_MAX);
        dispatch(input);
    }
}