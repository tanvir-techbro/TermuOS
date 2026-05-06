#include "fat32.h"
#include "../drivers/storage/disk.h"
#include "../lib/printf.h"
#include "../mm/heap.h"
#include "../lib/string.h"

static vfs_ops_t fat32_ops;

// ─────────────────────────────────────────────────────────────
// FAT32 BOOT STRUCT
// ─────────────────────────────────────────────────────────────

typedef struct __attribute__((packed))
{
    uint8_t jump[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    uint32_t fat_size_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
} FAT32_Boot;

static FAT32_Boot bpb;
static uint32_t fat_start;
static uint32_t data_start;
static uint32_t part_lba;

// ─────────────────────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────────────────────

static uint32_t cluster_to_lba(uint32_t cluster)
{
    return data_start + ((cluster - 2) * bpb.sectors_per_cluster);
}

static uint32_t fat32_next_cluster(uint32_t cluster)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start + (fat_offset / 512);

    uint8_t buffer[512];
    disk_read(fat_sector, 1, buffer);

    uint32_t *entry = (uint32_t *)(buffer + (fat_offset % 512));
    return (*entry) & 0x0FFFFFFF;
}

// ─────────────────────────────────────────────────────────────
// INIT
// ─────────────────────────────────────────────────────────────

void fat32_init(uint32_t lba_start)
{
    part_lba = lba_start;

    disk_read(part_lba, 1, &bpb);

    fat_start = part_lba + bpb.reserved_sectors;
    data_start = fat_start + (bpb.fat_count * bpb.fat_size_32);

    kprintf("FAT32 loaded\n");
}

// ─────────────────────────────────────────────────────────────
// DIRECTORY ENTRY
// ─────────────────────────────────────────────────────────────

typedef struct __attribute__((packed))
{
    char name[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t acc_date;
    uint16_t high_cluster;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t low_cluster;
    uint32_t size;
} DirEntry;

// ─────────────────────────────────────────────────────────────
// READ OP
// ─────────────────────────────────────────────────────────────

static int fat32_read_op(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer)
{
    if (!node || node->inode < 2)
        return -1;

    if (offset != 0)
        return 0; // simple version

    uint32_t cluster = node->inode;
    uint32_t remaining = node->size;
    uint32_t total = 0;

    while (cluster < 0x0FFFFFF8 && remaining > 0 && total < size)
    {
        uint32_t lba = cluster_to_lba(cluster);

        uint8_t temp[512];
        disk_read(lba, 1, temp);

        for (uint32_t i = 0; i < 512 && remaining > 0 && total < size; i++)
        {
            buffer[total++] = temp[i];
            remaining--;
        }

        cluster = fat32_next_cluster(cluster);
    }

    return total;
}

// ─────────────────────────────────────────────────────────────
// FINDDIR (KEY FIXED PART)
// ─────────────────────────────────────────────────────────────

static vfs_node_t *fat32_finddir(vfs_node_t *node, const char *name)
{
    (void)node;

    // Remove leading '/'
    if (name[0] == '/')
        name++;

    uint8_t buffer[4096];

    uint32_t lba = cluster_to_lba(bpb.root_cluster);
    disk_read(lba, bpb.sectors_per_cluster, buffer);

    DirEntry *entries = (DirEntry *)buffer;

    for (int i = 0; i < 128; i++)
    {
        if (entries[i].name[0] == 0x00)
            break;

        if ((uint8_t)entries[i].name[0] == 0xE5)
            continue;

        if (entries[i].attr == 0x0F)
            continue;

        if (entries[i].attr & 0x08)
            continue;

        // Convert FAT 8.3 → normal name
        char fname[12];
        int j = 0;

        for (int k = 0; k < 8 && entries[i].name[k] != ' '; k++)
            fname[j++] = entries[i].name[k];

        if (entries[i].name[8] != ' ')
        {
            fname[j++] = '.';
            for (int k = 8; k < 11 && entries[i].name[k] != ' '; k++)
                fname[j++] = entries[i].name[k];
        }

        fname[j] = 0;

        // DEBUG
        kprintf("Comparing FAT='%s' with VFS='%s'\n", fname, name);

        // Case-insensitive compare
        int match = 1;

        for (int k = 0; fname[k] || name[k]; k++)
        {
            char a = fname[k];
            char b = name[k];

            if (a >= 'a' && a <= 'z')
                a -= 32;
            if (b >= 'a' && b <= 'z')
                b -= 32;

            if (a != b)
            {
                match = 0;
                break;
            }
        }

        if (match)
        {
            uint32_t cluster =
                (entries[i].high_cluster << 16) |
                entries[i].low_cluster;

            if (cluster < 2)
                continue;

            vfs_node_t *out = kmalloc(sizeof(vfs_node_t));
            if (!out)
                return NULL;

            strcpy(out->name, fname);
            out->type = VFS_FILE;
            out->size = entries[i].size;
            out->inode = cluster;

            out->ops = &fat32_ops;

            kprintf("OPEN %s -> cluster=%x size=%d\n", fname, cluster, entries[i].size);

            return out;
        }
    }

    return NULL;
}

// ─────────────────────────────────────────────────────────────
// GLOBAL OPS (IMPORTANT: NOT LOCAL)
// ─────────────────────────────────────────────────────────────

static vfs_ops_t fat32_ops = {
    .read = fat32_read_op,
    .write = NULL,
    .finddir = fat32_finddir,
    .readdir = NULL,
    .create = NULL,
    .unlink = NULL,
    .close = NULL,
};

// ─────────────────────────────────────────────────────────────
// ROOT
// ─────────────────────────────────────────────────────────────

vfs_node_t *fat32_create_root()
{
    vfs_node_t *root = kmalloc(sizeof(vfs_node_t));

    if (!root)
        return NULL;

    strcpy(root->name, "/");
    root->type = VFS_DIR;
    root->size = 0;
    root->inode = bpb.root_cluster;
    root->ops = &fat32_ops;

    return root;
}