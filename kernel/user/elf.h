#pragma once
#include <stdint.h>

// Returns entry point address or 0 on failure
uint64_t elf_load(const char *path);