#pragma once
#include <stdint.h>
#include <stdbool.h>

bool ata_init(void);
bool ata_read28(uint32_t lba, uint8_t sector_count, void *buffer);
bool ata_write28(uint32_t lba, uint8_t sector_count, const void *buffer);
void ata_ioman_register(void);
