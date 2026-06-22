#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"

static int udp_socket_dispatch(const sock_addr_t *src, const uint8_t *data, uint32_t len);

int udp_input(const uint8_t *buf, uint32_t len, netif_t *ni) {
    (void)ni;
    if (len < ETH_HLEN + IP_HLEN + UDP_HLEN) return -1;

    ip_hdr_t *ip = (ip_hdr_t *)(buf + ETH_HLEN);
    udp_hdr_t *udp = (udp_hdr_t *)((uint8_t *)ip + IP_HLEN);

    sock_addr_t src;
    memcpy(src.ip, ip->src_ip, IP_ALEN);
    src.port = udp->src_port;

    uint16_t udp_len = udp->length - UDP_HLEN;
    uint8_t *payload = (uint8_t *)udp + UDP_HLEN;

    return udp_socket_dispatch(&src, payload, udp_len);
}

int udp_send(netif_t *ni, const uint8_t *dest_ip, uint16_t src_port,
             uint16_t dest_port, const uint8_t *data, uint32_t data_len) {
    uint32_t total = UDP_HLEN + data_len;
    uint8_t *udp_data = (uint8_t *)kmalloc(total);
    if (!udp_data) return -1;
    memset(udp_data, 0, total);

    udp_hdr_t *udp = (udp_hdr_t *)udp_data;
    udp->src_port = src_port;
    udp->dest_port = dest_port;
    udp->length = total;

    if (data_len > 0) {
        memcpy(udp_data + UDP_HLEN, data, data_len);
    }

    uint16_t udp_len = total;
    udp->checksum = net_checksum_pseudo(ni->ip_addr, dest_ip, IP_PROTO_UDP,
                                         udp_len, udp_data, udp_len);

    int ret = ip_output(ni, dest_ip, IP_PROTO_UDP, udp_data, total);
    kfree(udp_data);
    return ret;
}

static int udp_socket_dispatch(const sock_addr_t *src, const uint8_t *data, uint32_t len) {
    extern socket_t sockets[];
    extern int socket_count;

    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].used && sockets[i].type == SOCK_DGRAM &&
            sockets[i].state == SOCK_CONNECTED &&
            sockets[i].local.port == src->port) {
            if (sockets[i].recv_tail + len < SOCKET_BUF_SIZE) {
                memcpy(sockets[i].recv_buf + sockets[i].recv_tail, data, len);
                sockets[i].recv_tail += len;
            }
            return 0;
        }
    }

    for (int i = 0; i < socket_count; i++) {
        if (sockets[i].used && sockets[i].type == SOCK_DGRAM &&
            sockets[i].state == SOCK_BOUND &&
            sockets[i].local.port == src->port) {
            memcpy(&sockets[i].remote, src, sizeof(sock_addr_t));
            sockets[i].state = SOCK_CONNECTED;
            if (sockets[i].recv_tail + len < SOCKET_BUF_SIZE) {
                memcpy(sockets[i].recv_buf + sockets[i].recv_tail, data, len);
                sockets[i].recv_tail += len;
            }
            return 0;
        }
    }
    return -1;
}
