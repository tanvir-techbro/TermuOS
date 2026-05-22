# TermuOS

TermuOS is an open-source x86_64 hobby operating system focused on a Linux-inspired terminal-first environment.
The operating system is written primarily in C and x86_64 Assembly and is designed for learning low-level systems programming, kernel development, and operating system architecture.

---

## Features

* x86_64 kernel
* Multitasking scheduler
* Virtual File System (VFS)
* Custom shell and terminal
* Memory management
* Networking support
* Modular kernel structure
* Custom drivers
* QEMU support

---

## Screenshots

> Screenshots coming later.

Example:

```md
![Boot Screen](screenshots/boot.png)
```

---

## Project Goals

TermuOS aims to:

* teach operating system development
* explore low-level programming
* provide a Linux-inspired environment
* experiment with kernel architecture and system design
* remain lightweight and educational

---

## Project Structure

```text
kernel/         Kernel core
arch/           Architecture-specific code
drivers/        Hardware drivers
fs/             File systems and VFS
mm/             Memory management
net/            Networking
shell/          Shell and terminal
lib/            Utility libraries
build/          Build output
```

---

## Requirements

You will need:

* clang or gcc
* nasm
* make
* xorriso
* qemu-system-x86_64

Ubuntu/Debian example:

```bash
sudo apt install clang nasm xorriso qemu-system-x86 make
```

---

## Building

Clone the repository:

```bash
git clone https://github.com/RonnieHarrod-cell/TermuOS.git
cd TermuOS
```

Build the OS:

```bash
make
```

---

## Running

Run in QEMU:

```bash
make run
```

---

## Contributing

Contributions are welcome.

Please read:

```text
CONTRIBUTING.md
```

before opening pull requests or issues.

---

## Roadmap

Planned features include:

* User-mode programs
* ELF executable loader
* Improved networking stack
* Better filesystem support
* SMP support
* GUI experiments
* Audio drivers
* Package management
* System calls expansion

---

## License

TermuOS is licensed under the MIT License.

See:

```text
LICENSE
```

for more information.

---

## Links

* Website: https://termuos.netlify.app
* GitHub: https://github.com/RonnieHarrod-cell/TermuOS

---

## Author

Created by Ronnie Harrod.
