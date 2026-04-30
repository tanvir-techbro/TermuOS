#include "elf.h"
#include "../fs/vfs.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

// ─── ELF64 structures ─────────────────────────────────────────────────────────

#define ELF_MAGIC 0x464c457f // "\x7fELF"
#define ET_EXEC 2
#define EM_X86_64 62
#define PT_LOAD 1

typedef struct
{
    uint32_t magic;
    uint8_t bits;   // 2 = 64-bit
    uint8_t endian; // 1 = little
    uint8_t version;
    uint8_t abi;
    uint8_t pad[8];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint64_t entry;
    uint64_t phoff; // program header offset
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf64_hdr_t;

typedef struct
{
    uint32_t type;
    uint32_t flags;
    uint64_t offset; // offset in file
    uint64_t vaddr;  // virtual address to load at
    uint64_t paddr;
    uint64_t filesz; // bytes in file
    uint64_t memsz;  // bytes in memory (may be > filesz, zero-fill rest)
    uint64_t align;
} __attribute__((packed)) elf64_phdr_t;

// ─── Load ─────────────────────────────────────────────────────────────────────

uint64_t elf_load(const char *path)
{
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0)
    {
        kprintf("elf: cannot open %s\n", path);
        return 0;
    }

    // Read ELF header
    elf64_hdr_t hdr;
    if (vfs_read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
    {
        kprintf("elf: short read on header\n");
        vfs_close(fd);
        return 0;
    }

    // Validate
    if (hdr.magic != ELF_MAGIC)
    {
        kprintf("elf: bad magic\n");
        vfs_close(fd);
        return 0;
    }
    if (hdr.bits != 2 || hdr.machine != EM_X86_64)
    {
        kprintf("elf: not a 64-bit x86 ELF\n");
        vfs_close(fd);
        return 0;
    }

    kprintf("elf: loading %s, entry=0x%x, %u segments\n",
            path, hdr.entry, hdr.phnum);

    // Get current page map
    uint64_t cr3;
    __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));

    // Read and process each program header
    for (int i = 0; i < hdr.phnum; i++)
    {
        elf64_phdr_t phdr;
        uint64_t phdr_off = hdr.phoff + i * hdr.phentsize;

        // Seek by reopening is too complex — read all phdrs sequentially
        // We already read past the ELF header, need to seek to phoff
        // Use a simple approach: read from offset via vfs_read with offset tracking
        // Since our vfs_read advances the offset, we need to seek back
        // Simple workaround: close and reopen, skip to the right offset
        vfs_close(fd);
        fd = vfs_open(path, O_RDONLY);

        // Skip to program header
        uint8_t skip_buf[64];
        uint64_t to_skip = phdr_off;
        while (to_skip > 0)
        {
            int chunk = to_skip > 64 ? 64 : (int)to_skip;
            int n = vfs_read(fd, skip_buf, chunk);
            if (n <= 0)
                break;
            to_skip -= n;
        }

        if (vfs_read(fd, &phdr, sizeof(phdr)) != sizeof(phdr))
            continue;
        if (phdr.type != PT_LOAD)
            continue;

        kprintf("elf: PT_LOAD vaddr=0x%x filesz=%u memsz=%u\n",
                phdr.vaddr, phdr.filesz, phdr.memsz);

        // Allocate and map pages for this segment
        uint64_t vaddr = phdr.vaddr & ~(uint64_t)(PAGE_SIZE - 1);
        uint64_t end = (phdr.vaddr + phdr.memsz + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

        for (uint64_t va = vaddr; va < end; va += PAGE_SIZE)
        {
            void *phys = pmm_alloc();
            if (!phys)
            {
                kprintf("elf: out of memory\n");
                vfs_close(fd);
                return 0;
            }
            // Zero the page
            uint8_t *p = (uint8_t *)PHYS_TO_VIRT(phys);
            for (int j = 0; j < PAGE_SIZE; j++)
                p[j] = 0;
            vmm_map(cr3, va, (uint64_t)phys, VMM_USER_FLAGS);
        }

        // Copy file data into the mapped region
        vfs_close(fd);
        fd = vfs_open(path, O_RDONLY);

        // Skip to segment data in file
        to_skip = phdr.offset;
        while (to_skip > 0)
        {
            int chunk = to_skip > 64 ? 64 : (int)to_skip;
            int n = vfs_read(fd, skip_buf, chunk);
            if (n <= 0)
                break;
            to_skip -= n;
        }

        // Read segment data directly into mapped virtual address
        uint8_t *dest = (uint8_t *)phdr.vaddr;
        uint64_t remaining = phdr.filesz;
        while (remaining > 0)
        {
            int chunk = remaining > 256 ? 256 : (int)remaining;
            int n = vfs_read(fd, dest, chunk);
            if (n <= 0)
                break;
            dest += n;
            remaining -= n;
        }
    }

    vfs_close(fd);
    kprintf("elf: loaded, entry=0x%x\n", hdr.entry);
    return hdr.entry;
}