// mkfs_tfs — formats a raw .img file as TFS
// Usage: mkfs_tfs disk.img [size_mb]
//
// Must stay in sync with kernel/fs/tfs.h

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TFS_MAGIC 0x54465300
#define TFS_VERSION 1
#define TFS_SECTOR_SIZE 512
#define TFS_BLOCK_SIZE 4096
#define TFS_BLOCK_SECTORS (TFS_BLOCK_SIZE / TFS_SECTOR_SIZE)
#define TFS_MAX_INODES 512
#define TFS_INODE_SIZE 64
#define TFS_MAX_DIRECT 10
#define TFS_NAME_MAX 48
#define TFS_DIRENT_SIZE 64

#define TFS_LBA_SUPER 0
#define TFS_LBA_BITMAP 1
#define TFS_BITMAP_SECTORS 8
#define TFS_LBA_INODES (TFS_LBA_BITMAP + TFS_BITMAP_SECTORS)
#define TFS_INODE_SECTORS ((TFS_MAX_INODES * TFS_INODE_SIZE) / TFS_SECTOR_SIZE)
#define TFS_LBA_DATA (TFS_LBA_INODES + TFS_INODE_SECTORS)

#define TFS_TYPE_FREE 0
#define TFS_TYPE_FILE 1
#define TFS_TYPE_DIR 2

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
    uint8_t _pad[492];
} __attribute__((packed)) tfs_super_t;

typedef struct
{
    uint8_t type;
    uint8_t _pad0[3];
    uint32_t size;
    uint32_t nlinks;
    uint32_t blocks[TFS_MAX_DIRECT];
    uint8_t _pad1[TFS_INODE_SIZE - 4 - 4 - 4 - TFS_MAX_DIRECT * 4];
} __attribute__((packed)) tfs_inode_t;

static FILE *img;

static void write_sector(uint32_t lba, const void *buf)
{
    fseek(img, (long)lba * TFS_SECTOR_SIZE, SEEK_SET);
    fwrite(buf, TFS_SECTOR_SIZE, 1, img);
}

static void write_inode(uint32_t ino, const tfs_inode_t *in)
{
    uint32_t sector = TFS_LBA_INODES + (ino * TFS_INODE_SIZE) / TFS_SECTOR_SIZE;
    uint32_t offset = (ino * TFS_INODE_SIZE) % TFS_SECTOR_SIZE;
    uint8_t buf[TFS_SECTOR_SIZE];
    // Read-modify-write
    fseek(img, (long)sector * TFS_SECTOR_SIZE, SEEK_SET);
    fread(buf, TFS_SECTOR_SIZE, 1, img);
    memcpy(buf + offset, in, sizeof(tfs_inode_t));
    fseek(img, (long)sector * TFS_SECTOR_SIZE, SEEK_SET);
    fwrite(buf, TFS_SECTOR_SIZE, 1, img);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "disk.img";
    int size_mb = argc > 2 ? atoi(argv[2]) : 64;

    img = fopen(path, "wb");
    if (!img)
    {
        perror("open");
        return 1;
    }

    // Total sectors in the image
    long total_sectors = (long)size_mb * 1024 * 1024 / TFS_SECTOR_SIZE;

    // Extend file to full size
    fseek(img, total_sectors * TFS_SECTOR_SIZE - 1, SEEK_SET);
    fputc(0, img);
    rewind(img);

    // Zero everything first
    uint8_t zero[TFS_SECTOR_SIZE];
    memset(zero, 0, TFS_SECTOR_SIZE);
    for (long i = 0; i < total_sectors; i++)
    {
        fseek(img, i * TFS_SECTOR_SIZE, SEEK_SET);
        fwrite(zero, TFS_SECTOR_SIZE, 1, img);
    }

    uint32_t data_sectors = (uint32_t)(total_sectors - TFS_LBA_DATA);
    uint32_t total_blocks = data_sectors / TFS_BLOCK_SECTORS;

    // Superblock
    tfs_super_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = TFS_MAGIC;
    sb.version = TFS_VERSION;
    sb.total_blocks = total_blocks;
    sb.free_blocks = total_blocks;
    sb.total_inodes = TFS_MAX_INODES;
    sb.free_inodes = TFS_MAX_INODES - 1;

    uint8_t sb_buf[TFS_SECTOR_SIZE];
    memset(sb_buf, 0, TFS_SECTOR_SIZE);
    memcpy(sb_buf, &sb, sizeof(sb));
    write_sector(TFS_LBA_SUPER, sb_buf);

    // Bitmap — mark block 0 as reserved (null sentinel)
    uint8_t bmap_sector[TFS_SECTOR_SIZE];
    memset(bmap_sector, 0, TFS_SECTOR_SIZE);
    bmap_sector[0] = 0x01;
    fseek(img, (long)TFS_LBA_BITMAP * TFS_SECTOR_SIZE, SEEK_SET);
    fwrite(bmap_sector, TFS_SECTOR_SIZE, 1, img);
    sb.free_blocks--;

    // Root inode (inode 0)
    tfs_inode_t root;
    memset(&root, 0, sizeof(root));
    root.type = TFS_TYPE_DIR;
    root.nlinks = 1;
    write_inode(0, &root);

    fclose(img);

    printf("mkfs_tfs: created %s (%d MB, %u data blocks)\n",
           path, size_mb, total_blocks);
    printf("  LBA layout:\n");
    printf("    Superblock : LBA %d\n", TFS_LBA_SUPER);
    printf("    Bitmap     : LBA %d-%d\n", TFS_LBA_BITMAP, TFS_LBA_BITMAP + TFS_BITMAP_SECTORS - 1);
    printf("    Inodes     : LBA %d-%d\n", TFS_LBA_INODES, TFS_LBA_INODES + (int)TFS_INODE_SECTORS - 1);
    printf("    Data       : LBA %d+\n", TFS_LBA_DATA);
    return 0;
}