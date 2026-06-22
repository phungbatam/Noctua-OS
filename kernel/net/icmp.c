#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"

int icmp_input(const uint8_t *buf, uint32_t len, netif_t *ni) {
    if (len < ETH_HLEN + IP_HLEN + sizeof(icmp_hdr_t)) return -1;

    ip_hdr_t *ip = (ip_hdr_t *)(buf + ETH_HLEN);
    icmp_hdr_t *icmp = (icmp_hdr_t *)((uint8_t *)ip + IP_HLEN);
    uint32_t icmp_len = ip->total_length - IP_HLEN;

    (void)icmp_len;

    if (icmp->type == 8 && icmp->code == 0) {
        uint32_t reply_len = ETH_HLEN + IP_HLEN + icmp_len;
        uint8_t *reply = (uint8_t *)kmalloc(reply_len);
        if (!reply) return -1;
        memset(reply, 0, reply_len);

        eth_frame_t *reth = (eth_frame_t *)reply;
        ip_hdr_t *rip = (ip_hdr_t *)(reply + ETH_HLEN);
        icmp_hdr_t *ricmp = (icmp_hdr_t *)(reply + ETH_HLEN + IP_HLEN);

        uint8_t dest_mac[ETH_ALEN];
        memcpy(dest_mac, buf, ETH_ALEN);
        memcpy(reth->dest_mac, buf, ETH_ALEN);
        memcpy(reth->src_mac, ni->hw_addr, ETH_ALEN);
        reth->type = ETH_P_IP;

        rip->ver_ihl = 0x45;
        rip->total_length = IP_HLEN + icmp_len;
        rip->ttl = 64;
        rip->protocol = IP_PROTO_ICMP;
        memcpy(rip->src_ip, ip->dest_ip, IP_ALEN);
        memcpy(rip->dest_ip, ip->src_ip, IP_ALEN);

        ricmp->type = 0;
        ricmp->code = 0;
        ricmp->id = icmp->id;
        ricmp->sequence = icmp->sequence;
        uint32_t payload_bytes = icmp_len - sizeof(icmp_hdr_t);
        if (payload_bytes > 0) {
            memcpy(ricmp->payload, icmp->payload, payload_bytes);
        }
        ricmp->checksum = 0;
        ricmp->checksum = net_checksum((uint16_t *)ricmp, icmp_len);

        rip->header_checksum = 0;
        rip->header_checksum = ip_checksum(rip);

        if (ni->send) {
            ni->send(ni, reply, reply_len);
        }
        kfree(reply);
    }

    return 0;
}

void icmp_ping(netif_t *ni, const uint8_t *dest_ip, int count) {
    uint32_t pkt_len = ETH_HLEN + IP_HLEN + sizeof(icmp_hdr_t) + 56;
    uint8_t *packet = (uint8_t *)kmalloc(pkt_len);
    if (!packet) return;

    for (int i = 0; i < count; i++) {
        memset(packet, 0, pkt_len);

        eth_frame_t *eth = (eth_frame_t *)packet;
        ip_hdr_t *ip = (ip_hdr_t *)(packet + ETH_HLEN);
        icmp_hdr_t *icmp = (icmp_hdr_t *)(packet + ETH_HLEN + IP_HLEN);

        uint8_t dest_mac[ETH_ALEN];
        if (arp_resolve(ni, dest_ip, dest_mac) < 0) {
            memset(dest_mac, 0xFF, ETH_ALEN);
        }

        memcpy(eth->dest_mac, dest_mac, ETH_ALEN);
        memcpy(eth->src_mac, ni->hw_addr, ETH_ALEN);
        eth->type = ETH_P_IP;

        ip->ver_ihl = 0x45;
        ip->total_length = IP_HLEN + sizeof(icmp_hdr_t) + 56;
        ip->ttl = 64;
        ip->protocol = IP_PROTO_ICMP;
        memcpy(ip->src_ip, ni->ip_addr, IP_ALEN);
        memcpy(ip->dest_ip, dest_ip, IP_ALEN);

        icmp->type = 8;
        icmp->id = 0x0001;
        icmp->sequence = i + 1;
        for (int j = 0; j < 56; j++) icmp->payload[j] = j + i;
        icmp->checksum = net_checksum((uint16_t *)icmp, sizeof(icmp_hdr_t) + 56);

        ip->header_checksum = ip_checksum(ip);

        if (ni->send) {
            ni->send(ni, packet, pkt_len);
        }

        char ip_str[16];
        net_ip_to_str((uint8_t *)dest_ip, ip_str);
        screen_term_write("PING ");
        screen_term_write(ip_str);
        screen_term_write(" seq=");
        char buf[8];
        int2str(i + 1, buf);
        screen_term_write(buf);
        screen_term_write("\n");

        for (volatile int w = 0; w < 1000000; w++);
    }

    kfree(packet);
}
