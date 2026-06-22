#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"

extern socket_t sockets[];
extern int socket_count;

static uint32_t tcp_generate_isn(void) {
    static uint32_t isn = 0x12345678;
    isn += 0x00010001;
    return isn;
}

static void tcp_send_segment(netif_t *ni, const uint8_t *dest_ip,
                             uint16_t src_port, uint16_t dest_port,
                             uint32_t seq, uint32_t ack, uint8_t flags,
                             const uint8_t *data, uint32_t data_len) {
    uint32_t tcp_hdr_sz = TCP_HLEN_MIN;
    uint32_t total = tcp_hdr_sz + data_len;
    uint8_t *segment = (uint8_t *)kmalloc(total);
    if (!segment) return;
    memset(segment, 0, total);

    tcp_hdr_t *tcp = (tcp_hdr_t *)segment;
    tcp->src_port = src_port;
    tcp->dest_port = dest_port;
    tcp->seq_num = seq;
    tcp->ack_num = ack;
    tcp->data_offset = (tcp_hdr_sz / 4) << 4;
    tcp->flags = flags;
    tcp->window = 0x4000;
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;

    if (data_len > 0) {
        memcpy(segment + tcp_hdr_sz, data, data_len);
    }

    tcp->checksum = net_checksum_pseudo(ni->ip_addr, dest_ip,
                                         IP_PROTO_TCP, total,
                                         segment, total);

    ip_output(ni, dest_ip, IP_PROTO_TCP, segment, total);
    kfree(segment);
}

int tcp_input(const uint8_t *buf, uint32_t len, netif_t *ni) {
    if (len < ETH_HLEN + IP_HLEN + TCP_HLEN_MIN) return -1;

    ip_hdr_t *ip = (ip_hdr_t *)(buf + ETH_HLEN);
    tcp_hdr_t *tcp = (tcp_hdr_t *)((uint8_t *)ip + IP_HLEN);

    uint16_t src_port = tcp->src_port;
    uint16_t dest_port = tcp->dest_port;
    uint32_t seq = tcp->seq_num;
    uint32_t ack = tcp->ack_num;
    uint8_t flags = tcp->flags;

    uint32_t tcp_hdr_sz = (tcp->data_offset >> 4) * 4;
    uint8_t *payload = (uint8_t *)tcp + tcp_hdr_sz;
    uint32_t payload_len = len - ETH_HLEN - IP_HLEN - tcp_hdr_sz;
    if (payload_len > (uint32_t)(ip->total_length - IP_HLEN - tcp_hdr_sz)) {
        payload_len = ip->total_length - IP_HLEN - tcp_hdr_sz;
    }

    for (int i = 0; i < socket_count; i++) {
        if (!sockets[i].used) continue;

        int match = 0;

        if (sockets[i].type == SOCK_STREAM &&
            sockets[i].state == SOCK_LISTENING &&
            sockets[i].local.port == dest_port) {
            /* SYN received on listening socket */
            if (flags & TCP_SYN && !(flags & TCP_ACK)) {
                socket_t *child = (socket_t *)kmalloc(sizeof(socket_t));
                if (!child) return -1;
                memset(child, 0, sizeof(socket_t));

                child->used = 1;
                child->domain = AF_INET;
                child->type = SOCK_STREAM;
                child->protocol = IPPROTO_TCP;
                child->state = SOCK_CONNECTED;
                child->tcp_state = TCP_SYN_RECEIVED;
                child->tcp_seq = tcp_generate_isn();
                child->tcp_ack = seq + 1;
                child->recv_buf = (uint8_t *)kmalloc(SOCKET_BUF_SIZE);
                child->send_buf = (uint8_t *)kmalloc(SOCKET_BUF_SIZE);
                memset(child->recv_buf, 0, SOCKET_BUF_SIZE);
                memset(child->send_buf, 0, SOCKET_BUF_SIZE);

                memcpy(child->remote.ip, ip->src_ip, IP_ALEN);
                child->remote.port = src_port;
                memcpy(&child->local, &sockets[i].local, sizeof(sock_addr_t));

                child->pending = sockets[i].pending;
                sockets[i].pending = child;

                tcp_send_segment(ni, ip->src_ip, dest_port, src_port,
                                 child->tcp_seq, child->tcp_ack,
                                 TCP_SYN | TCP_ACK, NULL, 0);
                child->tcp_seq++;
                return 0;
            }
            continue;
        }

        if (sockets[i].state == SOCK_CONNECTED &&
            sockets[i].local.port == dest_port &&
            sockets[i].remote.port == src_port &&
            memcmp(sockets[i].remote.ip, ip->src_ip, IP_ALEN) == 0) {
            match = 1;
        }

        if (sockets[i].state == SOCK_CONNECTING &&
            sockets[i].local.port == dest_port &&
            sockets[i].remote.port == src_port &&
            memcmp(sockets[i].remote.ip, ip->src_ip, IP_ALEN) == 0) {
            match = 1;
        }

        if (!match) continue;

        if (flags & TCP_RST) {
            sockets[i].state = SOCK_CLOSED;
            sockets[i].tcp_state = TCP_CLOSED;
            sockets[i].recv_closed = 1;
            return 0;
        }

        if (flags & TCP_SYN && flags & TCP_ACK) {
            sockets[i].tcp_ack = seq + 1;
            sockets[i].tcp_seq = ack;
            sockets[i].tcp_state = TCP_ESTABLISHED;
            sockets[i].state = SOCK_CONNECTED;

            tcp_send_segment(ni, ip->src_ip, dest_port, src_port,
                             sockets[i].tcp_seq, sockets[i].tcp_ack,
                             TCP_ACK, NULL, 0);
            sockets[i].tcp_seq++;
            return 0;
        }

        if (flags & TCP_ACK) {
            uint32_t acked = ack - sockets[i].tcp_seq;
            if (acked > 0 && acked <= (sockets[i].send_tail - sockets[i].send_head)) {
                sockets[i].send_head += acked;
                sockets[i].tcp_seq = ack;
            }
        }

        if (flags & TCP_PSH || flags & TCP_ACK) {
            if (payload_len > 0) {
                if (sockets[i].recv_tail + payload_len < SOCKET_BUF_SIZE) {
                    memcpy(sockets[i].recv_buf + sockets[i].recv_tail, payload, payload_len);
                    sockets[i].recv_tail += payload_len;
                }
                sockets[i].tcp_ack = seq + payload_len;

                tcp_send_segment(ni, ip->src_ip, dest_port, src_port,
                                 sockets[i].tcp_seq, sockets[i].tcp_ack,
                                 TCP_ACK, NULL, 0);
            }
        }

        if (flags & TCP_FIN) {
            sockets[i].recv_closed = 1;
            sockets[i].tcp_ack = seq + 1;
            sockets[i].tcp_state = TCP_CLOSE_WAIT;

            tcp_send_segment(ni, ip->src_ip, dest_port, src_port,
                             sockets[i].tcp_seq, sockets[i].tcp_ack,
                             TCP_ACK, NULL, 0);
            return 0;
        }

        break;
    }

    return 0;
}

int tcp_connect_internal(netif_t *ni, socket_t *sock) {
    sock->tcp_seq = tcp_generate_isn();
    sock->tcp_ack = 0;
    sock->tcp_state = TCP_SYN_SENT;
    sock->state = SOCK_CONNECTING;

    tcp_send_segment(ni, sock->remote.ip,
                     sock->local.port, sock->remote.port,
                     sock->tcp_seq, 0, TCP_SYN, NULL, 0);
    sock->tcp_seq++;

    for (int retry = 0; retry < 10; retry++) {
        for (volatile int w = 0; w < 2000000; w++);

        uint8_t buf[ETH_FRAME_MAX];
        uint32_t rlen = 0;
        if (ni->poll && ni->poll(ni, buf, &rlen) && rlen >= ETH_HLEN + IP_HLEN + TCP_HLEN_MIN) {
            ip_hdr_t *ip = (ip_hdr_t *)(buf + ETH_HLEN);
            if (ip->protocol == IP_PROTO_TCP) {
                tcp_input(buf, rlen, ni);
                if (sock->tcp_state == TCP_ESTABLISHED) {
                    return 0;
                }
            }
        }
    }

    return -1;
}

int tcp_send_internal(netif_t *ni, socket_t *sock, const uint8_t *data, uint32_t len) {
    uint32_t sent = 0;
    while (sent < len) {
        uint32_t chunk = len - sent;
        if (chunk > sock->tcp_mss) chunk = sock->tcp_mss;

        tcp_send_segment(ni, sock->remote.ip,
                         sock->local.port, sock->remote.port,
                         sock->tcp_seq + sent, sock->tcp_ack,
                         TCP_PSH | TCP_ACK, data + sent, chunk);

        sent += chunk;

        for (volatile int w = 0; w < 100000; w++);

        uint8_t buf[ETH_FRAME_MAX];
        uint32_t rlen = 0;
        if (ni->poll && ni->poll(ni, buf, &rlen) && rlen >= ETH_HLEN + IP_HLEN + TCP_HLEN_MIN) {
            ip_hdr_t *ip = (ip_hdr_t *)(buf + ETH_HLEN);
            if (ip->protocol == IP_PROTO_TCP) {
                tcp_input(buf, rlen, ni);
            }
        }
    }

    sock->tcp_seq += sent;
    sock->send_head += sent;
    return sent;
}

int tcp_close_internal(netif_t *ni, socket_t *sock) {
    sock->tcp_state = TCP_FIN_WAIT1;
    sock->state = SOCK_CLOSING;

    tcp_send_segment(ni, sock->remote.ip,
                     sock->local.port, sock->remote.port,
                     sock->tcp_seq, sock->tcp_ack,
                     TCP_FIN | TCP_ACK, NULL, 0);
    sock->tcp_seq++;

    return 0;
}
