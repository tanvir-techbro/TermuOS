#include "net.h"
#include "../lib/printf.h"
#include <stdint.h>
#include <stddef.h>

netif_t netif = {0};

// ─── Byte order ───────────────────────────────────────────────────────────────

uint16_t net_htons(uint16_t x) { return (x >> 8) | (x << 8); }
uint32_t net_htonl(uint32_t x)
{
    return ((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >> 8) | ((x & 0x0000ff00) << 8) | ((x & 0x000000ff) << 24);
}

// ─── Checksum ─────────────────────────────────────────────────────────────────

static uint16_t checksum(const void *data, size_t len)
{
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1)
    {
        sum += *p++;
        len -= 2;
    }
    if (len)
        sum += *(const uint8_t *)p;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

// ─── Packet buffer ────────────────────────────────────────────────────────────

static uint8_t tx_buf[2048];

// ─── ARP ──────────────────────────────────────────────────────────────────────

// Simple ARP table (4 entries)
#define ARP_TABLE_SIZE 4
static struct
{
    ip4_t ip;
    mac_t mac;
    int valid;
} arp_table[ARP_TABLE_SIZE];

static void arp_table_set(ip4_t ip, mac_t mac)
{
    for (int i = 0; i < ARP_TABLE_SIZE; i++)
    {
        if (!arp_table[i].valid ||
            arp_table[i].ip.b[0] == ip.b[0] && arp_table[i].ip.b[1] == ip.b[1] &&
                arp_table[i].ip.b[2] == ip.b[2] && arp_table[i].ip.b[3] == ip.b[3])
        {
            arp_table[i].ip = ip;
            arp_table[i].mac = mac;
            arp_table[i].valid = 1;
            return;
        }
    }
}

static int arp_table_get(ip4_t ip, mac_t *mac_out)
{
    for (int i = 0; i < ARP_TABLE_SIZE; i++)
    {
        if (arp_table[i].valid &&
            arp_table[i].ip.b[0] == ip.b[0] && arp_table[i].ip.b[1] == ip.b[1] &&
            arp_table[i].ip.b[2] == ip.b[2] && arp_table[i].ip.b[3] == ip.b[3])
        {
            *mac_out = arp_table[i].mac;
            return 0;
        }
    }
    return -1;
}

void net_send_arp_request(ip4_t target_ip)
{
    eth_hdr_t *eth = (eth_hdr_t *)tx_buf;
    arp_pkt_t *arp = (arp_pkt_t *)(tx_buf + sizeof(eth_hdr_t));

    // Broadcast ethernet
    for (int i = 0; i < 6; i++)
        eth->dst.b[i] = 0xff;
    eth->src = netif.mac;
    eth->ethertype = net_htons(ETH_ARP);

    arp->htype = net_htons(1);
    arp->ptype = net_htons(ETH_IPV4);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = net_htons(1); // request
    arp->sha = netif.mac;
    arp->spa = netif.ip;
    for (int i = 0; i < 6; i++)
        arp->tha.b[i] = 0;
    arp->tpa = target_ip;

    if (netif.send)
        netif.send(tx_buf, sizeof(eth_hdr_t) + sizeof(arp_pkt_t));
}

static void handle_arp(const eth_hdr_t *eth, const arp_pkt_t *arp)
{
    // Learn sender
    arp_table_set(arp->spa, arp->sha);

    uint16_t oper = net_htons(arp->oper);
    if (oper == 1)
    {
        // Is it for us?
        if (arp->tpa.b[0] != netif.ip.b[0] || arp->tpa.b[1] != netif.ip.b[1] ||
            arp->tpa.b[2] != netif.ip.b[2] || arp->tpa.b[3] != netif.ip.b[3])
            return;

        // Send ARP reply
        eth_hdr_t *reth = (eth_hdr_t *)tx_buf;
        arp_pkt_t *rarp = (arp_pkt_t *)(tx_buf + sizeof(eth_hdr_t));

        reth->dst = eth->src;
        reth->src = netif.mac;
        reth->ethertype = net_htons(ETH_ARP);

        rarp->htype = net_htons(1);
        rarp->ptype = net_htons(ETH_IPV4);
        rarp->hlen = 6;
        rarp->plen = 4;
        rarp->oper = net_htons(2); // reply
        rarp->sha = netif.mac;
        rarp->spa = netif.ip;
        rarp->tha = arp->sha;
        rarp->tpa = arp->spa;

        if (netif.send)
            netif.send(tx_buf, sizeof(eth_hdr_t) + sizeof(arp_pkt_t));

        kprintf("net: ARP reply sent to " IP_FMT "\n", IP_ARGS(arp->spa));
    }
    else if (oper == 2)
    {
        kprintf("net: ARP reply from " IP_FMT " = " MAC_FMT "\n",
                IP_ARGS(arp->spa), MAC_ARGS(arp->sha));
    }
}

// ─── ICMP ─────────────────────────────────────────────────────────────────────

static ip4_t route(ip4_t dst)
{
    for (int i = 0; i < 4; i++)
        if ((dst.b[i] & netif.netmask.b[i]) != (netif.ip.b[i] & netif.netmask.b[i]))
            return netif.gateway;
    return dst;
}

void net_send_icmp_echo(ip4_t dst, uint16_t id, uint16_t seq)
{
    mac_t dst_mac;
    ip4_t nexthop = route(dst);
    if (arp_table_get(nexthop, &dst_mac) < 0)
    {
        kprintf("net: no ARP entry for " IP_FMT ", sending ARP request\n",
                IP_ARGS(nexthop));
        net_send_arp_request(nexthop);
        return;
    }

    eth_hdr_t *eth = (eth_hdr_t *)tx_buf;
    ip4_hdr_t *ip = (ip4_hdr_t *)(tx_buf + sizeof(eth_hdr_t));
    icmp_hdr_t *icmp = (icmp_hdr_t *)(tx_buf + sizeof(eth_hdr_t) + sizeof(ip4_hdr_t));

    eth->dst = dst_mac;
    eth->src = netif.mac;
    eth->ethertype = net_htons(ETH_IPV4);

    ip->ver_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->total_len = net_htons(sizeof(ip4_hdr_t) + sizeof(icmp_hdr_t));
    ip->id = net_htons(id);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->proto = IP_PROTO_ICMP;
    ip->checksum = 0;
    ip->src = netif.ip;
    ip->dst = dst;
    ip->checksum = checksum(ip, sizeof(ip4_hdr_t));

    icmp->type = 8; // echo request
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = net_htons(id);
    icmp->seq = net_htons(seq);
    icmp->checksum = checksum(icmp, sizeof(icmp_hdr_t));

    if (netif.send)
        netif.send(tx_buf, sizeof(eth_hdr_t) + sizeof(ip4_hdr_t) + sizeof(icmp_hdr_t));

    kprintf("net: ICMP echo sent to " IP_FMT "\n", IP_ARGS(dst));
}

static void handle_icmp(const ip4_hdr_t *ip, const icmp_hdr_t *icmp)
{
    if (icmp->type == 0)
    {
        kprintf("net: ICMP echo reply from " IP_FMT " seq=%u\n",
                IP_ARGS(ip->src), net_htons(icmp->seq));
    }
    else if (icmp->type == 8)
    {
        // Echo request — send reply
        mac_t dst_mac;
        if (arp_table_get(ip->src, &dst_mac) < 0)
            return;

        eth_hdr_t *reth = (eth_hdr_t *)tx_buf;
        ip4_hdr_t *rip = (ip4_hdr_t *)(tx_buf + sizeof(eth_hdr_t));
        icmp_hdr_t *ricmp = (icmp_hdr_t *)(tx_buf + sizeof(eth_hdr_t) + sizeof(ip4_hdr_t));

        reth->dst = dst_mac;
        reth->src = netif.mac;
        reth->ethertype = net_htons(ETH_IPV4);

        *rip = *ip;
        rip->src = netif.ip;
        rip->dst = ip->src;
        rip->checksum = 0;
        rip->checksum = checksum(rip, sizeof(ip4_hdr_t));

        *ricmp = *icmp;
        ricmp->type = 0; // reply
        ricmp->checksum = 0;
        ricmp->checksum = checksum(ricmp, sizeof(icmp_hdr_t));

        if (netif.send)
            netif.send(tx_buf, sizeof(eth_hdr_t) + sizeof(ip4_hdr_t) + sizeof(icmp_hdr_t));
    }
}

// ─── UDP ──────────────────────────────────────────────────────────────────────

void net_send_udp(ip4_t dst, uint16_t src_port, uint16_t dst_port,
                  const void *data, size_t len)
{
    mac_t dst_mac;
    if (arp_table_get(dst, &dst_mac) < 0)
    {
        net_send_arp_request(dst);
        return;
    }

    eth_hdr_t *eth = (eth_hdr_t *)tx_buf;
    ip4_hdr_t *ip = (ip4_hdr_t *)(tx_buf + sizeof(eth_hdr_t));
    udp_hdr_t *udp = (udp_hdr_t *)(tx_buf + sizeof(eth_hdr_t) + sizeof(ip4_hdr_t));
    uint8_t *pay = tx_buf + sizeof(eth_hdr_t) + sizeof(ip4_hdr_t) + sizeof(udp_hdr_t);

    eth->dst = dst_mac;
    eth->src = netif.mac;
    eth->ethertype = net_htons(ETH_IPV4);

    ip->ver_ihl = 0x45;
    ip->dscp_ecn = 0;
    ip->total_len = net_htons(sizeof(ip4_hdr_t) + sizeof(udp_hdr_t) + len);
    ip->id = 0;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->proto = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src = netif.ip;
    ip->dst = dst;
    ip->checksum = checksum(ip, sizeof(ip4_hdr_t));

    udp->src_port = net_htons(src_port);
    udp->dst_port = net_htons(dst_port);
    udp->length = net_htons(sizeof(udp_hdr_t) + len);
    udp->checksum = 0;

    const uint8_t *d = (const uint8_t *)data;
    for (size_t i = 0; i < len && i < sizeof(tx_buf) - 64; i++)
        pay[i] = d[i];

    if (netif.send)
        netif.send(tx_buf, sizeof(eth_hdr_t) + sizeof(ip4_hdr_t) + sizeof(udp_hdr_t) + len);
}

static void handle_udp(const ip4_hdr_t *ip, const udp_hdr_t *udp)
{
    uint16_t dst_port = net_htons(udp->dst_port);
    uint16_t len = net_htons(udp->length) - sizeof(udp_hdr_t);
    const uint8_t *data = (const uint8_t *)udp + sizeof(udp_hdr_t);
    kprintf("net: UDP from " IP_FMT ":%u -> port %u (%u bytes)\n",
            IP_ARGS(ip->src), net_htons(udp->src_port), dst_port, len);
    (void)data;
}

// ─── Receive dispatch ─────────────────────────────────────────────────────────

void net_receive(const void *frame, size_t len)
{
    if (len < sizeof(eth_hdr_t))
        return;
    const eth_hdr_t *eth = (const eth_hdr_t *)frame;
    uint16_t etype = net_htons(eth->ethertype);

    if (etype == ETH_ARP)
    {
        if (len < sizeof(eth_hdr_t) + sizeof(arp_pkt_t))
            return;
        handle_arp(eth, (const arp_pkt_t *)(eth + 1));
    }
    else if (etype == ETH_IPV4)
    {
        if (len < sizeof(eth_hdr_t) + sizeof(ip4_hdr_t))
            return;
        const ip4_hdr_t *ip = (const ip4_hdr_t *)(eth + 1);
        if (ip->proto == IP_PROTO_ICMP)
        {
            if (len < sizeof(eth_hdr_t) + sizeof(ip4_hdr_t) + sizeof(icmp_hdr_t))
                return;
            handle_icmp(ip, (const icmp_hdr_t *)(ip + 1));
        }
        else if (ip->proto == IP_PROTO_UDP)
        {
            if (len < sizeof(eth_hdr_t) + sizeof(ip4_hdr_t) + sizeof(udp_hdr_t))
                return;
            handle_udp(ip, (const udp_hdr_t *)(ip + 1));
        }
    }
}

void net_init(void)
{
    kprintf("net: stack initialised.\n");
    kprintf("net: MAC " MAC_FMT "\n", MAC_ARGS(netif.mac));
    kprintf("net: IP  " IP_FMT "\n", IP_ARGS(netif.ip));
}