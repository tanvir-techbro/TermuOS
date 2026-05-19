#pragma once
#include "desktop.hpp"
#include "window.hpp"

#define TERM_COLS 80
#define TERM_ROWS 20
#define TERM_PAD  4

class TerminalApp : public App {
public:
    TerminalApp();
    void launch() override;

    // Write a string into the terminal buffer (for kprintf redirect)
    static void puts(const char *s);
    void putchar(char c);

    static TerminalApp *_instance;

private:
    char _buf[TERM_ROWS * TERM_COLS];
    int  _cx, _cy;
    char _input[TERM_COLS];
    int  _input_len;
    int  _handle;

    void scroll();
    void print_prompt();
    void do_draw();
    void do_key(char c);

    // Window subclass so we can override on_key/on_draw
    class TermWindow : public Window {
    public:
        TerminalApp *app;
        TermWindow(TerminalApp *app, int x, int y, int w, int h);
        void on_key(char c) override;
    };
};