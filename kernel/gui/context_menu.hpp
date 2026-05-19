#pragma once
#include <stdint.h>

#ifdef __cplusplus

#define CONTEXT_MENU_MAX_ITEMS 8
#define CONTEXT_MENU_ITEM_H    20
#define CONTEXT_MENU_W         160
#define CONTEXT_MENU_PAD       4

struct ContextMenuItem
{
    const char *label;
    void (*on_click)(void *userdata);
    void *userdata;
    bool seperator; // if true, draw a line instead
};

class ContextMenu
{
public:
    ContextMenu();

    void add_item(const char *label, void (*on_click)(void *), void *userdata = nullptr);
    void add_separator();

    // Show at screen position
    void show(int x, int y);
    void hide();
    bool visible() const { return _visible; }

    // Draw the menu
    void draw();

    // Hit test - returns true if click was inside menu (consumed)
    bool on_click(int mx, int my);

    // Dismiss if click is outside
    bool on_click_outside(int mx, int my);

    int menu_h() const;
private:
    ContextMenuItem _items[CONTEXT_MENU_MAX_ITEMS];
    int _count;
    int _x, _y;
    bool _visible;
    int _hovered; // hovered item index
    
    int item_screen_y(int i) const;
};

#endif // __cplusplus

