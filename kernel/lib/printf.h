#pragma once

void kprintf(const char *fmt, ...);
void kprintf_set_output(void (*putchar_fn)(char));