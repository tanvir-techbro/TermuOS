#include "elf.h"
#include "userspace.h"

#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../lib/printf.h"

#define PAGE_SIZE 0x1000
#define PT_TLS    7

/* Exported to syscall.c so mmap/brk can map into the active user pagemap */
pagemap_t current_user_pm = 0;

static int elf_validate(Elf64_Ehdr *hdr)
{
    uint32_t magic = *(uint32_t *)hdr->e_ident;
    if (magic != ELF_MAGIC)   return 0;
    if (hdr->e_ident[4] != ELFCLASS64) return 0;
    if (hdr->e_machine != EM_X86_64)   return 0;
    if (hdr->e_type != ET_EXEC)        return 0;
    return 1;
}

static void map_segment(pagemap_t pm, uint64_t virt, uint64_t size)
{
    uint64_t start = virt & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t end   = (virt + size + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE)
    {
        void *phys = pmm_alloc();
        if (!phys) { kprintf("ELF: out of pages\n"); return; }
        vmm_map(pm, addr, (uint64_t)phys, VMM_PRESENT | VMM_WRITE | VMM_USER);
    }
}

static void setup_user_stack(pagemap_t pm)
{
    uint64_t start = USER_STACK_TOP - USER_STACK_SIZE;
    for (uint64_t addr = start; addr < USER_STACK_TOP; addr += PAGE_SIZE)
    {
        void *phys = pmm_alloc();
        if (!phys) { kprintf("ELF: out of stack pages\n"); return; }
        vmm_map(pm, addr, (uint64_t)phys, VMM_PRESENT | VMM_WRITE | VMM_USER);
    }
}

int elf_load(const char *path)
{
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) { kprintf("ELF: failed to open %s\n", path); return -1; }

    uint32_t type;
    uint64_t file_size;
    if (vfs_stat(path, &type, &file_size) < 0 || file_size == 0)
    {
        kprintf("ELF: stat failed %s\n", path);
        vfs_close(fd);
        return -1;
    }

    uint8_t *buf = (uint8_t *)kmalloc(file_size);
    if (!buf) { kprintf("ELF: OOM\n"); vfs_close(fd); return -1; }

    int n = vfs_read(fd, buf, file_size);
    vfs_close(fd);

    if (n < 0 || (uint64_t)n != file_size)
    {
        kprintf("ELF: read error %d / %llu\n", n, file_size);
        kfree(buf);
        return -1;
    }

    if (file_size < sizeof(Elf64_Ehdr) || !elf_validate((Elf64_Ehdr *)buf))
    {
        kprintf("ELF: invalid\n");
        kfree(buf);
        return -1;
    }

    Elf64_Ehdr *hdr = (Elf64_Ehdr *)buf;
    kprintf("ELF: entry=0x%llx phnum=%u\n", hdr->e_entry, hdr->e_phnum);

    pagemap_t user_pm = vmm_new_pagemap();
    uint64_t seg_top = 0;

    for (uint16_t i = 0; i < hdr->e_phnum; i++)
    {
        uint64_t off = hdr->e_phoff + (uint64_t)i * sizeof(Elf64_Phdr);
        if (off + sizeof(Elf64_Phdr) > file_size) continue;

        Elf64_Phdr *ph = (Elf64_Phdr *)(buf + off);
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_offset + ph->p_filesz > file_size) continue;

        kprintf("ELF: LOAD vaddr=0x%llx filesz=%llu memsz=%llu off=0x%llx\n",
                ph->p_vaddr, ph->p_filesz, ph->p_memsz, ph->p_offset);
        map_segment(user_pm, ph->p_vaddr, ph->p_memsz);

        /* Track highest mapped address for brk */
        uint64_t seg_end = (ph->p_vaddr + ph->p_memsz + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
        if (seg_end > seg_top) seg_top = seg_end;

        /* Copy file data via HHDM while still on kernel pagemap */
        uint8_t *src = buf + ph->p_offset;
        uint64_t virt = ph->p_vaddr;
        uint64_t done = 0;

        while (done < ph->p_filesz)
        {
            uint64_t pv   = (virt + done) & ~(uint64_t)(PAGE_SIZE - 1);
            uint64_t poff = (virt + done) &  (uint64_t)(PAGE_SIZE - 1);
            uint64_t chunk = PAGE_SIZE - poff;
            if (done + chunk > ph->p_filesz) chunk = ph->p_filesz - done;

            uint64_t phys = vmm_virt_to_phys(user_pm, pv);
            if (!phys) { kprintf("ELF: virt_to_phys failed at 0x%llx\n", pv); kfree(buf); return -1; }
            uint8_t *dst  = (uint8_t *)PHYS_TO_VIRT(phys) + poff;
            memcpy(dst, src + done, chunk);
            done += chunk;
        }

        /* Zero BSS */
        uint64_t z = virt + ph->p_filesz, ze = virt + ph->p_memsz;
        while (z < ze)
        {
            uint64_t pv    = z & ~(uint64_t)(PAGE_SIZE - 1);
            uint64_t poff  = z &  (uint64_t)(PAGE_SIZE - 1);
            uint64_t chunk = PAGE_SIZE - poff;
            if (z + chunk > ze) chunk = ze - z;
            uint64_t phys  = vmm_virt_to_phys(user_pm, pv);
            if (!phys) { kprintf("ELF: virt_to_phys BSS failed at 0x%llx\n", pv); kfree(buf); return -1; }
            uint8_t *dst   = (uint8_t *)PHYS_TO_VIRT(phys) + poff;
            memset(dst, 0, chunk);
            z += chunk;
        }
    }

    /* Set brk to just above highest loaded segment */
    extern uint64_t current_brk;
    current_brk = seg_top;
    kprintf("ELF: seg_top=0x%llx\n", seg_top);

    setup_user_stack(user_pm);

    /*
     * musl TLS setup — allocate a TCB block and point fs at it.
     * musl's static TLS layout (x86-64):
     *   [tls_data copy][padding][tcb]  where tcb starts at a 16-byte aligned addr
     *   and tcb[0] = pointer to itself (self pointer required by ABI).
     *
     * Find PT_TLS to get tls filesz/memsz/align.
     */
    uint64_t tls_filesz = 0, tls_memsz = 0, tls_src_vaddr = 0;
    for (uint16_t i = 0; i < hdr->e_phnum; i++)
    {
        uint64_t off = hdr->e_phoff + (uint64_t)i * sizeof(Elf64_Phdr);
        if (off + sizeof(Elf64_Phdr) > file_size) continue;
        Elf64_Phdr *ph = (Elf64_Phdr *)(buf + off);
        if (ph->p_type == PT_TLS)
        {
            tls_filesz    = ph->p_filesz;
            tls_memsz     = ph->p_memsz;
            tls_src_vaddr = ph->p_vaddr;
            break;
        }
    }

    /*
     * Allocate kernel-side TCB: [tls_memsz bytes][8 bytes self-ptr]
     * We allocate via PMM and map it into userspace, then set fs.
     * Total size rounded to pages.
     */
    uint64_t tcb_total = tls_memsz + 8;
    uint64_t tcb_pages = (tcb_total + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t tcb_virt  = USER_STACK_TOP + PAGE_SIZE; /* just above stack guard */

    for (uint64_t p = 0; p < tcb_pages; p++)
    {
        void *phys = pmm_alloc();
        if (!phys) { kprintf("ELF: TLS alloc failed\n"); kfree(buf); return -1; }
        /* Zero via HHDM */
        uint8_t *kva = (uint8_t *)PHYS_TO_VIRT((uint64_t)phys);
        for (uint64_t b = 0; b < PAGE_SIZE; b++) kva[b] = 0;
        vmm_map(user_pm, tcb_virt + p * PAGE_SIZE, (uint64_t)phys,
                VMM_PRESENT | VMM_WRITE | VMM_USER);
    }

    /* Copy TLS initialisation image (.tdata) from loaded segment */
    if (tls_filesz > 0 && tls_src_vaddr != 0)
    {
        for (uint64_t b = 0; b < tls_filesz; b++)
        {
            uint64_t src_pv   = (tls_src_vaddr + b) & ~(uint64_t)(PAGE_SIZE - 1);
            uint64_t src_poff = (tls_src_vaddr + b) &  (uint64_t)(PAGE_SIZE - 1);
            uint64_t src_phys = vmm_virt_to_phys(user_pm, src_pv);
            uint8_t  val      = src_phys ? ((uint8_t *)PHYS_TO_VIRT(src_phys))[src_poff] : 0;

            uint64_t dst_pv   = (tcb_virt + b) & ~(uint64_t)(PAGE_SIZE - 1);
            uint64_t dst_poff = (tcb_virt + b) &  (uint64_t)(PAGE_SIZE - 1);
            uint64_t dst_phys = vmm_virt_to_phys(user_pm, dst_pv);
            if (dst_phys)
                ((uint8_t *)PHYS_TO_VIRT(dst_phys))[dst_poff] = val;
        }
    }

    /* Write self-pointer: tcb->self = &tcb (musl x86-64 ABI requirement) */
    uint64_t tcb_addr = tcb_virt + tls_memsz;
    {
        uint64_t sp_pv   = tcb_addr & ~(uint64_t)(PAGE_SIZE - 1);
        uint64_t sp_poff = tcb_addr &  (uint64_t)(PAGE_SIZE - 1);
        uint64_t sp_phys = vmm_virt_to_phys(user_pm, sp_pv);
        if (sp_phys)
            *(uint64_t *)((uint8_t *)PHYS_TO_VIRT(sp_phys) + sp_poff) = tcb_addr;
    }

    kfree(buf);

    /* Make pagemap available to syscall mmap/brk handlers */
    current_user_pm = user_pm;

    vmm_switch(user_pm);

    /* Enable FSGSBASE so wrfsbase/rdfsbase work in ring 0 and ring 3 */
    uint64_t cr4;
    __asm__ volatile("movq %%cr4,%0" : "=r"(cr4));
    cr4 |= (1ULL << 16);
    __asm__ volatile("movq %0,%%cr4" :: "r"(cr4));

    /* Set fs base to TCB address (musl reads fs:0 as self-pointer) */
    __asm__ volatile("wrfsbase %0" :: "r"(tcb_addr) : "memory");

    kprintf("ELF: entering userspace entry=0x%llx tcb=0x%llx\n",
            hdr->e_entry, tcb_addr);
    jump_userspace(hdr->e_entry, USER_STACK_TOP);
    return 0;
}