#pragma once
#include <stdint.h>

void keyboard_init(void);
char keyboard_getchar(void); // blocking — waits for a keypress
int keyboard_haschar(void);  // non-blocking — returns 1 if key available