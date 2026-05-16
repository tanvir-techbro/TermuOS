import os
import curses

CONFIG_FILE = ".config"

options = {
    "CONFIG_NET": {
        "enabled": "y",
        "title": "Networking",
        "help": "Enables PCI networking, ARP, ICMP and virtio-net."
    },

    "CONFIG_VFS": {
        "enabled": "y",
        "title": "Virtual File System",
        "help": "Provides filesystem abstraction and RAMFS support."
    },

    "CONFIG_SHELL": {
        "enabled": "y",
        "title": "Shell",
        "help": "Enables the TermuOS command shell."
    },

    "CONFIG_USERSPACE": {
        "enabled": "y",
        "title": "Userspace Support",
        "help": "Enables syscalls and userspace task execution."
    },
}

# Load existing config
if os.path.exists(CONFIG_FILE):
    with open(CONFIG_FILE) as f:
        for line in f:
            line = line.strip()

            if "=" not in line:
                continue

            k, v = line.split("=", 1)

            if k in options:
                options[k]["enabled"] = v

def save():
    with open(CONFIG_FILE, "w") as f:
        for k, v in options.items():
            f.write(f"{k}={v['enabled']}\n")

def menu(stdscr):
    curses.curs_set(0)

    keys = list(options.keys())
    selected = 0

    while True:
        stdscr.clear()

        h, w = stdscr.getmaxyx()

        title = "TermuOS menuconfig"
        stdscr.addstr(1, (w - len(title)) // 2, title)

        for i, key in enumerate(keys):
            enabled = options[key]["enabled"] == "y"

            mark = "[*]" if enabled else "[ ]"

            text = f"{mark} {options[key]['title']}"

            if i == selected:
                stdscr.attron(curses.A_REVERSE)

            stdscr.addstr(i + 3, 4, text)

            if i == selected:
                stdscr.attroff(curses.A_REVERSE)

        selected_key = keys[selected]

        help_text = options[selected_key]["help"]

        stdscr.addstr(h - 6, 2, "Help:")
        stdscr.addstr(h - 5, 4, help_text)

        stdscr.addstr(
            h - 2,
            2,
            "↑/↓ Move  SPACE Toggle  Q Save & Exit"
        )

        key = stdscr.getch()

        if key == curses.KEY_UP:
            selected = max(0, selected - 1)

        elif key == curses.KEY_DOWN:
            selected = min(len(keys) - 1, selected + 1)

        elif key == ord(' '):
            k = keys[selected]

            options[k]["enabled"] = (
                "n"
                if options[k]["enabled"] == "y"
                else "y"
            )

        elif key == ord('?'):
            stdscr.clear()

            selected_key = keys[selected]

            title = options[selected_key]["title"]
            help_text = options[selected_key]["help"]

            stdscr.addstr(2, 2, title, curses.A_BOLD)

            stdscr.addstr(4, 2, help_text)

            stdscr.addstr(h - 2, 2, "Press any key to return")

            stdscr.getch()

        elif key in [ord('q'), ord('Q')]:
            save()
            break

curses.wrapper(menu)