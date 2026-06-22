#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"

static netif_t *netif_list = NULL;
static int netif_count = 0;
static uint8_t local_ip[4] = {10, 0, 2, 15};

void net_init(void) {
    arp_cache_init();
    screen_term_write("[NET] Network subsystem initialized\n");
    char buf[16];
    net_ip_to_str(local_ip, buf);
    screen_term_write("[NET] Local IP: ");
    screen_term_write(buf);
    screen_term_write("\n");
}

void net_set_ip(const uint8_t *ip) {
    memcpy(local_ip, ip, 4);
}

void netif_register(netif_t *ni) {
    ni->next = netif_list;
    netif_list = ni;
    netif_count++;

    char mac_str[18];
    net_mac_to_str(ni->hw_addr, mac_str);

    char ip_str[16];
    net_ip_to_str(ni->ip_addr, ip_str);

    screen_term_write("[NET] Registered interface ");
    screen_term_write(ni->name);
    screen_term_write(" MAC=");
    screen_term_write(mac_str);
    screen_term_write(" IP=");
    screen_term_write(ip_str);
    screen_term_write("\n");
}

netif_t *netif_get(const char *name) {
    netif_t *ni = netif_list;
    while (ni) {
        if (strcmp(ni->name, name) == 0) return ni;
        ni = ni->next;
    }
    return NULL;
}
