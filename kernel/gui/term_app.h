#pragma once
#include "app.h"

// Returns the terminal app descriptor for registering with the desktop
const app_t *term_app_get(void);

// Called by the desktop when the terminal icon is clicked
void term_app_launch(void);

// Write a string into the terminal window buffer (for kprintf redirect)
void term_app_puts(const char *s);
