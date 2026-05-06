#include "ata.h"

#define ATA_DATA 0x1F0
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LO 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HI 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_CMD 0x1F7
#define ATA_STATUS 0x1F7

#define ATA_CMD_READ 0x20

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0,%1" ::"a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1,%0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int ata_wait()
{
    for (int i = 0; i < 1000000; i++)
    {
        uint8_t status = inb(ATA_STATUS);

        if (!(status & 0x80) && (status & 0x08))
        {
            return 1; // ready
        }
    }
    return 0; // timeout
}

bool ata_read28(uint32_t lba, uint8_t count, void *buffer)
{
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)lba >> 16);
    outb(ATA_CMD, ATA_CMD_READ);

    uint16_t *buf = (uint16_t *)buffer;

    for (int s = 0; s < count; s++)
    {
        if (!ata_wait()) return false;

        for (int i = 0; i < 256; i++)
        {
            buf[i] = inw(ATA_DATA);
        }
        buf += 256;
    }

    return true;
}

bool ata_init()
{
    return true; // minimal
}
