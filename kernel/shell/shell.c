#include "shell.h"
#include "../drivers/video/terminal.h"
#include "../drivers/input/keyboard.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../arch/x86_64/pit.h"
#include "../sched/scheduler.h"
#include "../fs/vfs.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

#define INPUT_MAX 256
#define MAX_ARGS 16
#define HOSTNAME "TermuOS"
#define USERNAME "root"

static char cwd[VFS_PATH_MAX] = "/";

// ─── String helpers ───────────────────────────────────────────────────────────

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

static void sh_strcpy(char *dst, const char *src, int max)
{
    int i = 0;
    while (src[i] && i < max - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void sh_memset(void *p, uint8_t v, int n)
{
    uint8_t *b = p;
    while (n--)
        *b++ = v;
}

// Build absolute path from cwd + arg
static void resolve_path(const char *arg, char *out, int max)
{
    if (arg[0] == '/')
    {
        sh_strcpy(out, arg, max);
    }
    else
    {
        int cl = sh_strlen(cwd);
        sh_strcpy(out, cwd, max);
        if (cwd[cl - 1] != '/')
        {
            out[cl] = '/';
            out[cl + 1] = '\0';
        }
        // append arg
        int ol = sh_strlen(out);
        int i = 0;
        while (arg[i] && ol + i < max - 1)
        {
            out[ol + i] = arg[i];
            i++;
        }
        out[ol + i] = '\0';
    }
}

// ─── Input ────────────────────────────────────────────────────────────────────

static int readline(char *buf, int max)
{
    int len = 0;
    while (1)
    {
        char c = keyboard_getchar();
        if (c == '\n')
        {
            terminal_putchar('\n');
            buf[len] = '\0';
            return len;
        }
        if (c == '\b')
        {
            if (len > 0)
            {
                len--;
                terminal_putchar('\b');
            }
            continue;
        }
        if (c == 3)
        {
            terminal_putchar('\n');
            buf[0] = '\0';
            return 0;
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
            *p++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

// ─── Commands ─────────────────────────────────────────────────────────────────

static void cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kprintf("Commands:\n");
    kprintf("  help              this message\n");
    kprintf("  clear             clear screen\n");
    kprintf("  echo [args]       print text\n");
    kprintf("  uname             OS info\n");
    kprintf("  uptime            time since boot\n");
    kprintf("  mem               memory usage\n");
    kprintf("  threads           list threads\n");
    kprintf("  ls [path]         list directory\n");
    kprintf("  cd <path>         change directory\n");
    kprintf("  pwd               print working dir\n");
    kprintf("  cat <file>        print file contents\n");
    kprintf("  write <f> <text>  write text to file\n");
    kprintf("  mkdir <dir>       create directory\n");
    kprintf("  rm <file>         delete file\n");
    kprintf("  touch <file>      create empty file\n");
    kprintf("  reboot            reboot system\n");
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
    kprintf("TermuOS 0.1.0 x86_64\n");
}

static void cmd_uptime(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    uint64_t t = pit_ticks();
    uint64_t secs = t / 100;
    uint64_t mins = secs / 60;
    uint64_t hours = mins / 60;
    kprintf("up %u:%02u:%02u\n", hours, mins % 60, secs % 60);
}

static void cmd_mem(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    size_t free_p = pmm_free_pages();
    size_t total_p = pmm_total_pages();
    size_t used_p = total_p - free_p;
    kprintf("Total: %u MB  Used: %u MB  Free: %u MB\n",
            (total_p * 4096) / (1024 * 1024),
            (used_p * 4096) / (1024 * 1024),
            (free_p * 4096) / (1024 * 1024));
}

static void cmd_threads(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    extern thread_t threads[MAX_THREADS];
    static const char *states[] = {"dead", "ready", "running", "blocked"};
    kprintf("%-4s %-16s %s\n", "ID", "NAME", "STATE");
    for (int i = 0; i < MAX_THREADS; i++)
    {
        if (threads[i].state == THREAD_DEAD)
            continue;
        kprintf("%-4u %-16s %s\n",
                threads[i].id, threads[i].name, states[threads[i].state]);
    }
}

static void cmd_pwd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kprintf("%s\n", cwd);
}

static void cmd_cd(int argc, char **argv)
{
    if (argc < 2)
    {
        kprintf("cd: missing argument\n");
        return;
    }
    char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    uint32_t type;
    if (vfs_stat(path, &type, NULL) < 0)
    {
        kprintf("cd: %s: no such directory\n", argv[1]);
        return;
    }
    if (type != VFS_DIR)
    {
        kprintf("cd: %s: not a directory\n", argv[1]);
        return;
    }
    sh_strcpy(cwd, path, VFS_PATH_MAX);
}

static void cmd_ls(int argc, char **argv)
{
    char path[VFS_PATH_MAX];
    if (argc < 2)
        sh_strcpy(path, cwd, VFS_PATH_MAX);
    else
        resolve_path(argv[1], path, VFS_PATH_MAX);

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0)
    {
        kprintf("ls: %s: not found\n", path);
        return;
    }

    char name[VFS_NAME_MAX];
    uint32_t idx = 0;
    while (vfs_readdir(fd, idx++, name) == 0)
    {
        char full[VFS_PATH_MAX];
        int pl = sh_strlen(path);
        sh_strcpy(full, path, VFS_PATH_MAX);
        if (pl > 0 && full[pl - 1] != '/')
        {
            full[pl] = '/';
            full[pl + 1] = '\0';
            pl++;
        }
        int ni = 0;
        while (name[ni] && pl + ni < VFS_PATH_MAX - 1)
        {
            full[pl + ni] = name[ni];
            ni++;
        }
        full[pl + ni] = '\0';

        uint32_t type = VFS_FILE;
        uint64_t size = 0;
        vfs_stat(full, &type, &size);

        if (type == VFS_DIR)
            kprintf("%s/\n", name);
        else
            kprintf("%s  (%u bytes)\n", name, size);
    }
    vfs_close(fd);
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        kprintf("cat: missing filename\n");
        return;
    }
    char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0)
    {
        kprintf("cat: %s: not found\n", argv[1]);
        return;
    }

    uint8_t buf[256];
    int n;
    while ((n = vfs_read(fd, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        terminal_puts((char *)buf);
    }
    terminal_putchar('\n');
    vfs_close(fd);
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 3)
    {
        kprintf("write: usage: write <file> <text>\n");
        return;
    }
    char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);

    int fd = vfs_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        kprintf("write: cannot open %s\n", argv[1]);
        return;
    }

    // Join remaining args with spaces
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
        kprintf("touch: missing filename\n");
        return;
    }
    char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    if (vfs_create(path) < 0)
        kprintf("touch: failed to create %s\n", argv[1]);
}

static void cmd_mkdir(int argc, char **argv)
{
    if (argc < 2)
    {
        kprintf("mkdir: missing dirname\n");
        return;
    }
    char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    if (vfs_mkdir(path) < 0)
        kprintf("mkdir: failed to create %s\n", argv[1]);
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        kprintf("rm: missing filename\n");
        return;
    }
    char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    if (vfs_unlink(path) < 0)
        kprintf("rm: %s: not found\n", argv[1]);
}

static void cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kprintf("Rebooting...\n");
    uint8_t v = 0;
    while (v & 0x02)
        __asm__ volatile("inb $0x64, %0" : "=a"(v));
    __asm__ volatile("outb %0, $0x64" ::"a"((uint8_t)0xfe));
    for (;;)
        __asm__ volatile("hlt");
}

// ─── exec command ────────────────────────────────────────────────────────────
extern int exec(const char *path);

static void cmd_exec(int argc, char **argv)
{
    if (argc < 2)
    {
        kprintf("exec: usage: exec <path>\n");
        return;
    }
    char path[VFS_PATH_MAX];
    resolve_path(argv[1], path, VFS_PATH_MAX);
    exec(path);
}

// ─── Dispatch ─────────────────────────────────────────────────────────────────

typedef struct
{
    const char *name;
    void (*fn)(int, char **);
} command_t;

static const command_t commands[] = {
    {"help", cmd_help},
    {"clear", cmd_clear},
    {"echo", cmd_echo},
    {"uname", cmd_uname},
    {"uptime", cmd_uptime},
    {"mem", cmd_mem},
    {"threads", cmd_threads},
    {"pwd", cmd_pwd},
    {"cd", cmd_cd},
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"write", cmd_write},
    {"touch", cmd_touch},
    {"mkdir", cmd_mkdir},
    {"rm", cmd_rm},
    {"reboot", cmd_reboot},
    {"exec", cmd_exec},
    {NULL, NULL}};

static void dispatch(char *line)
{
    if (!line || !line[0])
        return;
    char *argv[MAX_ARGS];
    int argc = parse_args(line, argv);
    if (!argc)
        return;
    for (int i = 0; commands[i].name; i++)
    {
        if (sh_strcmp(argv[0], commands[i].name) == 0)
        {
            commands[i].fn(argc, argv);
            return;
        }
    }
    kprintf("sh: command not found: %s\n", argv[0]);
}

// ─── Prompt ───────────────────────────────────────────────────────────────────

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

// ─── Entry ────────────────────────────────────────────────────────────────────

void shell_run(void)
{
    char input[INPUT_MAX];
    __asm__ volatile("sti"); // ensure interrupts enabled in this thread
    kprintf("\nTermuOS 0.1.0 -- type 'help' for commands.\n\n");
    while (1)
    {
        print_prompt();
        sh_memset(input, 0, sizeof(input));
        readline(input, INPUT_MAX);
        dispatch(input);
    }
}