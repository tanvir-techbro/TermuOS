#include "disk.h"
#include "ata.h"

bool disk_read(uint32_t lba, uint32_t count, void *buffer)
{
    return ata_read28(lba, count, buffer);
}