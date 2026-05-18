#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int x, y;
    int dx, dy;
    int left, right, middle;
} mouse_state_t;

void mouse_init(void);
void mouse_set_screen(int w, int h);
mouse_state_t mouse_get(void);
int mouse_moved(void);

#ifdef __cplusplus
}
#endif
