#pragma once
#include <stdint.h>
#include <stdbool.h>

bool disk_read(uint32_t lba, uint32_t count, void *buffer);
bool disk_write(uint32_t lba, uint32_t count, const void *buffer);
