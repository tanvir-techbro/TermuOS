#include "virtio_net.h"
#include "pci.h"
#include "../../net/net.h"
#include "../../arch/x86_64/idt.h"
#include "../../mm/pmm.h"
#include "../../mm/vmm.h"
#include "../../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

// ─── Virtio legacy register offsets ──────────────────────────────────────────
#define VIRTIO_PCI_HOST_FEATURES   0x00
#define VIRTIO_PCI_GUEST_FEATURES  0x04
#define VIRTIO_PCI_QUEUE_PFN       0x08
#define VIRTIO_PCI_QUEUE_SIZE      0x0C
#define VIRTIO_PCI_QUEUE_SEL       0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY    0x10
#define VIRTIO_PCI_STATUS          0x12
#define VIRTIO_PCI_ISR             0x13
#define VIRTIO_PCI_CONFIG          0x14

#define VIRTIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4
#define VIRTIO_NET_F_MAC           (1 << 5)

#define VRING_DESC_F_NEXT   1
#define VRING_DESC_F_WRITE  2

// Fixed queue size — must match what device reports or be smaller
#define QUEUE_SIZE  16

// ─── Virtqueue structs ────────────────────────────────────────────────────────

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) vring_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
    uint16_t used_event;
} __attribute__((packed)) vring_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) vring_used_elem_t;

typedef struct {
    uint16_t          flags;
    uint16_t          idx;
    vring_used_elem_t ring[QUEUE_SIZE];
    uint16_t          avail_event;
} __attribute__((packed)) vring_used_t;

typedef struct {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed)) virtio_net_hdr_t;

// ─── Queue layout in a 4KB page ──────────────────────────────────────────────
// We fit everything in 2 pages:
// Page 0: descriptors (16 * 16 = 256 bytes) + avail ring
// Page 1: used ring (page-aligned as required by spec)

typedef struct {
    vring_desc_t  desc[QUEUE_SIZE];
    vring_avail_t avail;
    uint8_t       _pad[PAGE_SIZE - sizeof(vring_desc_t)*QUEUE_SIZE - sizeof(vring_avail_t)];
    vring_used_t  used;
} __attribute__((packed, aligned(PAGE_SIZE))) vring_mem_t;

// Statically allocated virtqueue memory (in BSS — guaranteed mapped)
static vring_mem_t rxq_mem __attribute__((aligned(PAGE_SIZE)));
static vring_mem_t txq_mem __attribute__((aligned(PAGE_SIZE)));

typedef struct {
    vring_desc_t  *desc;
    vring_avail_t *avail;
    vring_used_t  *used;
    uint16_t       last_used;
    uint16_t       free_head;
    uint16_t       num_free;
} virtq_t;

static virtq_t   rxq, txq;
static uint16_t  io_base;

// RX buffers — statically allocated
#define RX_BUF_SIZE 1536
static virtio_net_hdr_t rx_hdrs[QUEUE_SIZE] __attribute__((aligned(16)));
static uint8_t          rx_bufs[QUEUE_SIZE][RX_BUF_SIZE] __attribute__((aligned(16)));

// TX header buffer
static virtio_net_hdr_t tx_hdr __attribute__((aligned(16)));

// ─── I/O port helpers ─────────────────────────────────────────────────────────

static inline void outb(uint16_t p, uint8_t v)  { __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline void outl(uint16_t p, uint32_t v) { __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t  inb(uint16_t p) { uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint16_t inw(uint16_t p) { uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p)); return v; }

// ─── Physical address of a kernel virtual address ────────────────────────────
// For BSS/data symbols we can use virt - hhdm_base, but simpler:
// Limine identity-maps the kernel's physical pages, so phys = virt - kernel_virt_offset
// We use vmm_virt_to_phys on the current page map.

static uint64_t kvirt_to_phys(void *virt)
{
    uint64_t cr3;
    __asm__ volatile("movq %%cr3,%0":"=r"(cr3));
    return vmm_virt_to_phys(cr3, (uint64_t)virt);
}

// ─── Queue init ───────────────────────────────────────────────────────────────

static int init_queue(uint16_t idx, virtq_t *q, vring_mem_t *mem)
{
    outw(io_base + VIRTIO_PCI_QUEUE_SEL, idx);
    uint16_t size = inw(io_base + VIRTIO_PCI_QUEUE_SIZE);
    kprintf("virtio-net: queue %u hw_size=%u, using %u\n", idx, size, QUEUE_SIZE);
    if (!size) return -1;

    // Zero the memory
    uint8_t *p = (uint8_t *)mem;
    for (size_t i = 0; i < sizeof(vring_mem_t); i++) p[i] = 0;

    q->desc      = mem->desc;
    q->avail     = &mem->avail;
    q->used      = &mem->used;
    q->last_used = 0;
    q->free_head = 0;
    q->num_free  = QUEUE_SIZE;

    // Link descriptor free chain
    for (int i = 0; i < QUEUE_SIZE - 1; i++)
        q->desc[i].next = i + 1;

    // Get physical address of the descriptor table (must be 4096-aligned)
    uint64_t phys = kvirt_to_phys(mem);
    kprintf("virtio-net: queue %u phys=0x%x\n", idx, phys);

    if (phys & 0xfff) {
        kprintf("virtio-net: queue memory not page-aligned!\n");
        return -1;
    }

    // Tell device: PFN = phys / 4096
    outl(io_base + VIRTIO_PCI_QUEUE_PFN, phys / PAGE_SIZE);
    return 0;
}

// ─── RX refill ────────────────────────────────────────────────────────────────

static void rx_refill(int slot)
{
    if (rxq.num_free < 2) return;

    uint16_t d0 = rxq.free_head;
    rxq.free_head = rxq.desc[d0].next;
    rxq.num_free--;

    uint16_t d1 = rxq.free_head;
    rxq.free_head = rxq.desc[d1].next;
    rxq.num_free--;

    rxq.desc[d0].addr  = kvirt_to_phys(&rx_hdrs[slot]);
    rxq.desc[d0].len   = sizeof(virtio_net_hdr_t);
    rxq.desc[d0].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    rxq.desc[d0].next  = d1;

    rxq.desc[d1].addr  = kvirt_to_phys(rx_bufs[slot]);
    rxq.desc[d1].len   = RX_BUF_SIZE;
    rxq.desc[d1].flags = VRING_DESC_F_WRITE;
    rxq.desc[d1].next  = 0;

    rxq.avail->ring[rxq.avail->idx % QUEUE_SIZE] = d0;
    __asm__ volatile("" ::: "memory");
    rxq.avail->idx++;
    __asm__ volatile("" ::: "memory");
}

// ─── TX send ─────────────────────────────────────────────────────────────────

// Separate TX data buffer — packet goes here before send
static uint8_t tx_data_buf[2048] __attribute__((aligned(16)));

static void virtio_send(const void *data, size_t len)
{
    if (txq.num_free < 2) { kprintf("virtio-net: tx full\n"); return; }
    if (len > sizeof(tx_data_buf)) { kprintf("virtio-net: packet too large\n"); return; }

    // Copy packet to our static buffer (ensures physical address is stable)
    uint8_t *dst = tx_data_buf;
    const uint8_t *src = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) dst[i] = src[i];

    // Zero net header
    uint8_t *h = (uint8_t *)&tx_hdr;
    for (size_t i = 0; i < sizeof(tx_hdr); i++) h[i] = 0;

    uint16_t d0 = txq.free_head;
    txq.free_head = txq.desc[d0].next;
    txq.num_free--;

    uint16_t d1 = txq.free_head;
    txq.free_head = txq.desc[d1].next;
    txq.num_free--;

    txq.desc[d0].addr  = kvirt_to_phys(&tx_hdr);
    txq.desc[d0].len   = sizeof(virtio_net_hdr_t);
    txq.desc[d0].flags = VRING_DESC_F_NEXT;
    txq.desc[d0].next  = d1;

    txq.desc[d1].addr  = kvirt_to_phys(tx_data_buf);
    txq.desc[d1].len   = len;
    txq.desc[d1].flags = 0;
    txq.desc[d1].next  = 0;

    txq.avail->ring[txq.avail->idx % QUEUE_SIZE] = d0;
    __asm__ volatile("" ::: "memory");
    txq.avail->idx++;
    __asm__ volatile("" ::: "memory");

    outw(io_base + VIRTIO_PCI_QUEUE_NOTIFY, 1);
}

// ─── IRQ ─────────────────────────────────────────────────────────────────────

static void virtio_irq(registers_t *r)
{
    (void)r;
    uint8_t isr = inb(io_base + VIRTIO_PCI_ISR);
    if (!(isr & 1)) return;

    // Process RX
    while (rxq.last_used != rxq.used->idx) {
        vring_used_elem_t *e = &rxq.used->ring[rxq.last_used % QUEUE_SIZE];
        uint16_t d    = e->id;
        uint32_t plen = e->len - sizeof(virtio_net_hdr_t);

        // Find which slot this descriptor belongs to
        for (int i = 0; i < QUEUE_SIZE; i++) {
            if (rxq.desc[d].addr == kvirt_to_phys(&rx_hdrs[i])) {
                net_receive(rx_bufs[i], plen);
                // Return descriptors to free list
                rxq.desc[d].next = rxq.free_head;
                uint16_t d1 = rxq.desc[d].next;
                rxq.free_head = d;
                rxq.num_free += 2;
                (void)d1;
                rx_refill(i);
                break;
            }
        }
        rxq.last_used++;
    }

    // Reclaim TX
    while (txq.last_used != txq.used->idx) {
        vring_used_elem_t *e = &txq.used->ring[txq.last_used % QUEUE_SIZE];
        uint16_t d = e->id;
        uint16_t flags = txq.desc[d].flags;
        uint16_t next  = txq.desc[d].next;
        txq.desc[d].next = txq.free_head;
        txq.free_head = d;
        txq.num_free++;
        if (flags & VRING_DESC_F_NEXT) {
            txq.desc[next].next = txq.free_head;
            txq.free_head = next;
            txq.num_free++;
        }
        txq.last_used++;
    }
}

// ─── Init ─────────────────────────────────────────────────────────────────────

int virtio_net_init(void)
{
    pci_device_t dev;
    if (pci_find(PCI_VENDOR_VIRTIO, PCI_DEVICE_NET, &dev) < 0) {
        kprintf("virtio-net: not found\n");
        return -1;
    }

    io_base = dev.bar[0] & ~0x3u;
    kprintf("virtio-net: io_base=0x%x irq=%u\n", io_base, dev.irq);
    pci_enable_busmaster(&dev);

    // Reset + init sequence
    outb(io_base + VIRTIO_PCI_STATUS, 0);
    outb(io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    outb(io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // Negotiate: only MAC feature
    uint32_t host_feat = 0;
    // Read 4 bytes properly
    __asm__ volatile("inl %1,%0":"=a"(host_feat):"Nd"((uint16_t)(io_base + VIRTIO_PCI_HOST_FEATURES)));
    kprintf("virtio-net: host features=0x%x\n", host_feat);
    uint32_t our_feat = host_feat & VIRTIO_NET_F_MAC;
    __asm__ volatile("outl %0,%1"::"a"(our_feat),"Nd"((uint16_t)(io_base + VIRTIO_PCI_GUEST_FEATURES)));

    // Read MAC
    for (int i = 0; i < 6; i++)
        netif.mac.b[i] = inb(io_base + VIRTIO_PCI_CONFIG + i);
    kprintf("virtio-net: MAC " MAC_FMT "\n", MAC_ARGS(netif.mac));

    // Setup queues
    if (init_queue(0, &rxq, &rxq_mem) < 0) return -1;
    if (init_queue(1, &txq, &txq_mem) < 0) return -1;

    // Fill RX queue
    for (int i = 0; i < QUEUE_SIZE / 2; i++) rx_refill(i);
    outw(io_base + VIRTIO_PCI_QUEUE_NOTIFY, 0);

    // Register IRQ
    idt_register_irq(dev.irq, virtio_irq);

    // Signal driver ready
    outb(io_base + VIRTIO_PCI_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    // Configure network interface
    netif.ip.b[0]=10; netif.ip.b[1]=0;  netif.ip.b[2]=2;  netif.ip.b[3]=15;
    netif.gateway.b[0]=10; netif.gateway.b[1]=0; netif.gateway.b[2]=2; netif.gateway.b[3]=2;
    netif.send = virtio_send;

    net_init();
    kprintf("virtio-net: ready.\n");
    return 0;
}