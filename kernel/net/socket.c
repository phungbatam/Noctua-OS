#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"

#define MAX_SOCKETS 64

socket_t sockets[MAX_SOCKETS];
int socket_count = 0;

static int alloc_sock(void) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].used) {
            sockets[i].used = 1;
            if (i + 1 > socket_count) socket_count = i + 1;
            return i;
        }
    }
    return -1;
}

static void free_sock(int fd) {
    if (fd < 0 || fd >= MAX_SOCKETS || !sockets[fd].used) return;
    if (sockets[fd].recv_buf) {
        kfree(sockets[fd].recv_buf);
        sockets[fd].recv_buf = NULL;
    }
    if (sockets[fd].send_buf) {
        kfree(sockets[fd].send_buf);
        sockets[fd].send_buf = NULL;
    }
    memset(&sockets[fd], 0, sizeof(socket_t));
}

static uint16_t get_ephemeral_port(void) {
    static uint16_t port = 49152;
    port++;
    if (port < 49152) port = 49152;
    return port;
}

int socket_create(int domain, int type, int protocol) {
    if (domain != AF_INET) return -1;
    if (type != SOCK_STREAM && type != SOCK_DGRAM) return -1;

    int fd = alloc_sock();
    if (fd < 0) return -1;

    sockets[fd].domain = domain;
    sockets[fd].type = type;
    sockets[fd].protocol = protocol;
    sockets[fd].state = SOCK_OPEN;
    sockets[fd].tcp_state = TCP_CLOSED;
    sockets[fd].tcp_mss = 1460;
    sockets[fd].nonblock = 0;

    return fd;
}

int socket_bind(int sockfd, const sock_addr_t *addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;

    memcpy(&sockets[sockfd].local, addr, sizeof(sock_addr_t));
    sockets[sockfd].state = SOCK_BOUND;
    return 0;
}

int socket_listen(int sockfd, int backlog) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;
    if (sockets[sockfd].type != SOCK_STREAM) return -1;

    if (backlog > 16) backlog = 16;
    sockets[sockfd].state = SOCK_LISTENING;
    sockets[sockfd].tcp_state = TCP_LISTEN;
    sockets[sockfd].backlog_count = backlog;

    return 0;
}

int socket_accept(int sockfd, sock_addr_t *addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;
    if (sockets[sockfd].state != SOCK_LISTENING) return -1;

    while (!sockets[sockfd].pending) {
        netif_t *ni = netif_get("eth0");
        if (ni && ni->poll) {
            uint8_t buf[ETH_FRAME_MAX];
            uint32_t len = 0;
            if (ni->poll(ni, buf, &len) && len > 0) {
                eth_frame_t *eth = (eth_frame_t *)buf;
                if (eth->type == ETH_P_IP) {
                    ip_input(buf, len, ni);
                } else if (eth->type == ETH_P_ARP) {
                    arp_handle_packet(buf, len, ni);
                }
            }
        }
        if (!sockets[sockfd].pending) {
            return -1;
        }
    }

    socket_t *child = sockets[sockfd].pending;
    sockets[sockfd].pending = child->pending;
    child->pending = NULL;

    int newfd = alloc_sock();
    if (newfd < 0) {
        kfree(child);
        return -1;
    }

    memcpy(&sockets[newfd], child, sizeof(socket_t));
    sockets[newfd].pending = NULL;
    kfree(child);

    if (addr) {
        memcpy(addr, &sockets[newfd].remote, sizeof(sock_addr_t));
    }

    return newfd;
}

int socket_connect(int sockfd, const sock_addr_t *addr) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;

    memcpy(&sockets[sockfd].remote, addr, sizeof(sock_addr_t));

    if (sockets[sockfd].local.port == 0) {
        sockets[sockfd].local.port = get_ephemeral_port();
    }

    netif_t *ni = NULL;
    if (ip_route(addr->ip, &ni) < 0) return -1;

    if (sockets[sockfd].type == SOCK_DGRAM) {
        sockets[sockfd].state = SOCK_CONNECTED;
        sockets[sockfd].recv_buf = (uint8_t *)kmalloc(SOCKET_BUF_SIZE);
        sockets[sockfd].send_buf = (uint8_t *)kmalloc(SOCKET_BUF_SIZE);
        if (!sockets[sockfd].recv_buf || !sockets[sockfd].send_buf) return -1;
        return 0;
    }

    if (sockets[sockfd].type == SOCK_STREAM) {
        sockets[sockfd].recv_buf = (uint8_t *)kmalloc(SOCKET_BUF_SIZE);
        sockets[sockfd].send_buf = (uint8_t *)kmalloc(SOCKET_BUF_SIZE);
        if (!sockets[sockfd].recv_buf || !sockets[sockfd].send_buf) return -1;
        memset(sockets[sockfd].recv_buf, 0, SOCKET_BUF_SIZE);
        memset(sockets[sockfd].send_buf, 0, SOCKET_BUF_SIZE);

        return tcp_connect_internal(ni, &sockets[sockfd]);
    }

    return -1;
}

int socket_send(int sockfd, const uint8_t *buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;
    if (sockets[sockfd].state != SOCK_CONNECTED) return -1;

    netif_t *ni = NULL;
    if (ip_route(sockets[sockfd].remote.ip, &ni) < 0) return -1;

    if (sockets[sockfd].type == SOCK_DGRAM) {
        return udp_send(ni, sockets[sockfd].remote.ip,
                        sockets[sockfd].local.port, sockets[sockfd].remote.port,
                        buf, len);
    }

    if (sockets[sockfd].type == SOCK_STREAM) {
        return tcp_send_internal(ni, &sockets[sockfd], buf, len);
    }

    return -1;
}

int socket_recv(int sockfd, uint8_t *buf, uint32_t len, int flags) {
    (void)flags;
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;
    if (sockets[sockfd].state != SOCK_CONNECTED && sockets[sockfd].state != SOCK_CLOSING) return -1;

    if (sockets[sockfd].type == SOCK_STREAM || sockets[sockfd].type == SOCK_DGRAM) {
        netif_t *ni = netif_get("eth0");
        if (ni && ni->poll) {
            uint8_t pkt[ETH_FRAME_MAX];
            uint32_t plen = 0;
            while (ni->poll(ni, pkt, &plen) && plen > 0) {
                eth_frame_t *eth = (eth_frame_t *)pkt;
                if (eth->type == ETH_P_IP) {
                    ip_input(pkt, plen, ni);
                } else if (eth->type == ETH_P_ARP) {
                    arp_handle_packet(pkt, plen, ni);
                }
                plen = 0;
            }
        }

        uint32_t avail = sockets[sockfd].recv_tail - sockets[sockfd].recv_head;
        if (avail == 0) {
            if (sockets[sockfd].recv_closed) return 0;
            if (sockets[sockfd].nonblock) return -1;
            return 0;
        }

        uint32_t to_copy = (len < avail) ? len : avail;
        memcpy(buf, sockets[sockfd].recv_buf + sockets[sockfd].recv_head, to_copy);
        sockets[sockfd].recv_head += to_copy;

        if (sockets[sockfd].recv_head == sockets[sockfd].recv_tail) {
            sockets[sockfd].recv_head = 0;
            sockets[sockfd].recv_tail = 0;
        }

        return to_copy;
    }

    return -1;
}

int socket_close(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;

    if (sockets[sockfd].type == SOCK_STREAM &&
        sockets[sockfd].state == SOCK_CONNECTED) {
        netif_t *ni = netif_get("eth0");
        if (ni) {
            tcp_close_internal(ni, &sockets[sockfd]);
        }
    }

    free_sock(sockfd);
    return 0;
}

int socket_setopt(int sockfd, int level, int optname, const void *optval, int optlen) {
    (void)level;
    (void)optlen;

    if (sockfd < 0 || sockfd >= MAX_SOCKETS || !sockets[sockfd].used) return -1;
    if (optname == 0) return 0;

    return 0;
}
