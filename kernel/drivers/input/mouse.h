#pragma once
#include <stdint.h>

typedef struct
{
    int x, y;
    int dx, dy;
    int left, right, middle;
} mouse_state_t;

void mouse_init(void);
mouse_state_t mouse_get(void);
int mouse_moved(void); // returns 1 if state changed since last call
