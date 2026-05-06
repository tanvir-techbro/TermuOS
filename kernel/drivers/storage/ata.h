#pragma once
#include <stdint.h>
#include <stdbool.h>

bool ata_init();
bool ata_read28(uint32_t lba, uint8_t sector_count, void *buffer);
