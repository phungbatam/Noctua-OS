#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"
#include "ports.h"
#include "drivers/net/rtl8139.h"

#define ARP_CACHE_SIZE 32

typedef struct {
    uint8_t ip[IP_ALEN];
    uint8_t mac[ETH_ALEN];
    int valid;
} arp_cache_entry_t;

static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];
static int arp_cache_next = 0;

void arp_cache_init(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
    arp_cache_next = 0;
}

int arp_cache_lookup(const uint8_t *ip, uint8_t *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid &&
            arp_cache[i].ip[0] == ip[0] &&
            arp_cache[i].ip[1] == ip[1] &&
            arp_cache[i].ip[2] == ip[2] &&
            arp_cache[i].ip[3] == ip[3]) {
            memcpy(mac, arp_cache[i].mac, ETH_ALEN);
            return 0;
        }
    }
    return -1;
}

void arp_cache_add(const uint8_t *ip, const uint8_t *mac) {
    int idx = arp_cache_next;
    arp_cache_next = (arp_cache_next + 1) % ARP_CACHE_SIZE;

    memcpy(arp_cache[idx].ip, ip, IP_ALEN);
    memcpy(arp_cache[idx].mac, mac, ETH_ALEN);
    arp_cache[idx].valid = 1;
}

static int arp_build_request(const uint8_t *target_ip, uint8_t *packet, netif_t *ni) {
    eth_frame_t *eth = (eth_frame_t *)packet;
    arp_pkt_t *arp = (arp_pkt_t *)(packet + ETH_HLEN);

    memset(eth->dest_mac, 0xFF, ETH_ALEN);
    memcpy(eth->src_mac, ni->hw_addr, ETH_ALEN);
    eth->type = ETH_P_ARP;

    arp->htype = 0x0100;
    arp->ptype = 0x0008;
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = ARP_REQUEST;
    memset(arp->sha, 0, ETH_ALEN);
    memcpy(arp->sha, ni->hw_addr, ETH_ALEN);
    memcpy(arp->spa, ni->ip_addr, IP_ALEN);
    memset(arp->tha, 0, ETH_ALEN);
    memcpy(arp->tpa, target_ip, IP_ALEN);

    return ETH_HLEN + sizeof(arp_pkt_t);
}

static int arp_build_reply(arp_pkt_t *req, uint8_t *packet, netif_t *ni) {
    eth_frame_t *eth = (eth_frame_t *)packet;
    arp_pkt_t *arp = (arp_pkt_t *)(packet + ETH_HLEN);

    memcpy(eth->dest_mac, req->sha, ETH_ALEN);
    memcpy(eth->src_mac, ni->hw_addr, ETH_ALEN);
    eth->type = ETH_P_ARP;

    arp->htype = 0x0100;
    arp->ptype = 0x0008;
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = ARP_REPLY;
    memcpy(arp->sha, ni->hw_addr, ETH_ALEN);
    memcpy(arp->spa, ni->ip_addr, IP_ALEN);
    memcpy(arp->tha, req->sha, ETH_ALEN);
    memcpy(arp->tpa, req->spa, IP_ALEN);

    return ETH_HLEN + sizeof(arp_pkt_t);
}

int arp_resolve(netif_t *ni, const uint8_t *target_ip, uint8_t *target_mac) {
    if (arp_cache_lookup(target_ip, target_mac) == 0) {
        return 0;
    }

    uint8_t packet[ETH_FRAME_MAX];
    int len = arp_build_request(target_ip, packet, ni);
    if (ni->send) {
        ni->send(ni, packet, len);
    }

    for (int retry = 0; retry < 5; retry++) {
        for (volatile int wait = 0; wait < 500000; wait++);

        uint8_t buf[ETH_FRAME_MAX];
        uint32_t rlen = 0;
        if (ni->poll && ni->poll(ni, buf, &rlen) && rlen >= ETH_HLEN + sizeof(arp_pkt_t)) {
            eth_frame_t *eth = (eth_frame_t *)buf;
            if (eth->type == ETH_P_ARP) {
                arp_pkt_t *arp = (arp_pkt_t *)(buf + ETH_HLEN);
                if (arp->oper == ARP_REPLY &&
                    arp->spa[0] == target_ip[0] &&
                    arp->spa[1] == target_ip[1] &&
                    arp->spa[2] == target_ip[2] &&
                    arp->spa[3] == target_ip[3]) {
                    memcpy(target_mac, arp->sha, ETH_ALEN);
                    arp_cache_add(target_ip, target_mac);
                    return 0;
                }
            }
        }
    }

    return -1;
}

int arp_handle_packet(const uint8_t *buf, uint32_t len, netif_t *ni) {
    if (len < ETH_HLEN + sizeof(arp_pkt_t)) return -1;

    arp_pkt_t *arp = (arp_pkt_t *)(buf + ETH_HLEN);

    if (arp->oper == ARP_REQUEST) {
        if (arp->tpa[0] == ni->ip_addr[0] &&
            arp->tpa[1] == ni->ip_addr[1] &&
            arp->tpa[2] == ni->ip_addr[2] &&
            arp->tpa[3] == ni->ip_addr[3]) {
            uint8_t reply[ETH_FRAME_MAX];
            int rlen = arp_build_reply(arp, reply, ni);
            if (ni->send) {
                ni->send(ni, reply, rlen);
            }
            arp_cache_add(arp->spa, arp->sha);
        }
    } else if (arp->oper == ARP_REPLY) {
        arp_cache_add(arp->spa, arp->sha);
    }

    return 0;
}
