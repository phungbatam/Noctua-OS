#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"

extern int ip_input(const uint8_t *buf, uint32_t len, netif_t *ni);

uint16_t ip_checksum(ip_hdr_t *ip) {
    return net_checksum((uint16_t *)ip, (ip->ver_ihl & 0x0F) * 4);
}

int ip_output(netif_t *ni, const uint8_t *dest_ip, uint8_t protocol,
              const uint8_t *data, uint32_t data_len) {
    uint32_t total = ETH_HLEN + IP_HLEN + data_len;
    uint8_t *packet = (uint8_t *)kmalloc(total);
    if (!packet) return -1;
    memset(packet, 0, total);

    eth_frame_t *eth = (eth_frame_t *)packet;
    ip_hdr_t *ip = (ip_hdr_t *)(packet + ETH_HLEN);

    uint8_t dest_mac[ETH_ALEN];
    if (arp_resolve(ni, dest_ip, dest_mac) < 0) {
        memset(dest_mac, 0xFF, ETH_ALEN);
    }

    memcpy(eth->dest_mac, dest_mac, ETH_ALEN);
    memcpy(eth->src_mac, ni->hw_addr, ETH_ALEN);
    eth->type = ETH_P_IP;

    ip->ver_ihl = 0x45;
    ip->total_length = IP_HLEN + data_len;
    ip->id = 0;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    memcpy(ip->src_ip, ni->ip_addr, IP_ALEN);
    memcpy(ip->dest_ip, dest_ip, IP_ALEN);
    ip->header_checksum = 0;
    ip->header_checksum = ip_checksum(ip);

    if (data_len > 0) {
        memcpy(packet + ETH_HLEN + IP_HLEN, data, data_len);
    }

    int ret = -1;
    if (ni->send) {
        ret = ni->send(ni, packet, total);
    }
    kfree(packet);
    return ret;
}

int ip_input(const uint8_t *buf, uint32_t len, netif_t *ni) {
    if (len < ETH_HLEN + IP_HLEN) return -1;

    ip_hdr_t *ip = (ip_hdr_t *)(buf + ETH_HLEN);
    uint16_t csum = ip->header_checksum;
    ip->header_checksum = 0;
    if (net_checksum((uint16_t *)ip, (ip->ver_ihl & 0x0F) * 4) != csum) {
        return -1;
    }
    ip->header_checksum = csum;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            return icmp_input(buf, len, ni);
        case IP_PROTO_UDP:
            return udp_input(buf, len, ni);
        case IP_PROTO_TCP:
            return tcp_input(buf, len, ni);
        default:
            return -1;
    }
}

int ip_route(const uint8_t *dest_ip, netif_t **out_ni) {
    (void)dest_ip;
    netif_t *ni = netif_get("eth0");
    if (!ni) return -1;
    *out_ni = ni;
    return 0;
}
