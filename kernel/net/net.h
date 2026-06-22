#ifndef NOCTUA_NET_H
#define NOCTUA_NET_H

#include <stdint.h>

#define ETH_ALEN        6
#define IP_ALEN         4
#define ETH_HLEN        14
#define IP_HLEN         20
#define UDP_HLEN        8
#define TCP_HLEN_MIN    20
#define ETH_FRAME_MAX   1514
#define MTU             1500

/* Ethernet protocol types */
#define ETH_P_IP        0x0800
#define ETH_P_ARP       0x0806

/* IP protocol types */
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

/* ARP operations */
#define ARP_REQUEST     1
#define ARP_REPLY       2

/* TCP flags */
#define TCP_FIN     0x01
#define TCP_SYN     0x02
#define TCP_RST     0x04
#define TCP_PSH     0x08
#define TCP_ACK     0x10
#define TCP_SYN_ACK (TCP_SYN | TCP_ACK)
#define TCP_FIN_ACK (TCP_FIN | TCP_ACK)

/* Socket types */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

/* Address families */
#define AF_INET         2

/* Protocol for socket() */
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_ICMP    1

/* Socket options */
#define SO_REUSEADDR    1
#define SO_BINDTODEVICE 2

/* Socket flags */
#define MSG_DONTWAIT    0x01
#define O_NONBLOCK      0x800

/* TCP states */
enum tcp_state {
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT,
    TCP_CLOSED
};

/* Socket state */
enum sock_state {
    SOCK_UNUSED,
    SOCK_OPEN,
    SOCK_BOUND,
    SOCK_LISTENING,
    SOCK_CONNECTING,
    SOCK_CONNECTED,
    SOCK_CLOSING,
    SOCK_CLOSED
};

/* ---- Packet structures (network byte order) ---- */

typedef struct {
    uint8_t  dest_mac[ETH_ALEN];
    uint8_t  src_mac[ETH_ALEN];
    uint16_t type;
    uint8_t  payload[];
} __attribute__((packed)) eth_frame_t;

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[ETH_ALEN];
    uint8_t  spa[IP_ALEN];
    uint8_t  tha[ETH_ALEN];
    uint8_t  tpa[IP_ALEN];
} __attribute__((packed)) arp_pkt_t;

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  dscp;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t header_checksum;
    uint8_t  src_ip[IP_ALEN];
    uint8_t  dest_ip[IP_ALEN];
    uint8_t  options[];
} __attribute__((packed)) ip_hdr_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
    uint8_t  payload[];
} __attribute__((packed)) icmp_hdr_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t  payload[];
} __attribute__((packed)) udp_hdr_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
    uint8_t  options[];
} __attribute__((packed)) tcp_hdr_t;

/* Pseudo header for TCP/UDP checksum calculation */
typedef struct {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} __attribute__((packed)) pseudo_hdr_t;

/* ---- Socket address structure ---- */
typedef struct {
    uint8_t  ip[IP_ALEN];
    uint16_t port;
} __attribute__((packed)) sock_addr_t;

/* ---- Network interface ---- */
#define IF_NAME_LEN     16
#define IF_IP_ALEN      4
#define IF_HW_ALEN      6

typedef struct netif {
    struct netif *next;
    char name[IF_NAME_LEN];
    uint8_t hw_addr[IF_HW_ALEN];
    uint8_t ip_addr[IF_IP_ALEN];
    uint8_t netmask[IF_IP_ALEN];
    uint8_t gateway[IF_IP_ALEN];
    uint32_t flags;
    int (*send)(struct netif *ni, const uint8_t *data, uint32_t len);
    int (*poll)(struct netif *ni, uint8_t *buffer, uint32_t *len);
} netif_t;

#define NETIF_F_UP       0x01
#define NETIF_F_LOOPBACK 0x02

/* ---- Socket structure ---- */
#define MAX_SOCKETS     64
#define SOCKET_BUF_SIZE 65536

typedef struct socket {
    int used;
    int domain;
    int type;
    int protocol;
    int state;
    int nonblock;

    sock_addr_t local;
    sock_addr_t remote;

    /* For TCP */
    enum tcp_state tcp_state;
    uint32_t tcp_seq;
    uint32_t tcp_ack;
    uint16_t tcp_mss;

    /* Buffers */
    uint8_t *recv_buf;
    uint32_t recv_head;
    uint32_t recv_tail;
    int recv_closed;

    uint8_t *send_buf;
    uint32_t send_head;
    uint32_t send_tail;

    /* Backlog for listening */
    struct socket *backlog[16];
    int backlog_count;

    /* Pending connection for accept */
    struct socket *pending;
} socket_t;

/* ---- Global functions ---- */
uint16_t net_checksum(uint16_t *data, int len);
uint16_t net_checksum_pseudo(const uint8_t *src_ip, const uint8_t *dest_ip,
                             uint8_t protocol, uint16_t tcp_len,
                             const uint8_t *data, int data_len);
void net_ip_to_str(uint8_t *ip, char *str);
int net_str_to_ip(const char *str, uint8_t *ip);
void net_mac_to_str(uint8_t *mac, char *str);

/* IP functions */
uint16_t ip_checksum(ip_hdr_t *ip);

/* Network initialization */
void net_init(void);
void net_set_ip(const uint8_t *ip);

/* ---- Socket API ---- */
int socket_create(int domain, int type, int protocol);
int socket_bind(int sockfd, const sock_addr_t *addr);
int socket_listen(int sockfd, int backlog);
int socket_accept(int sockfd, sock_addr_t *addr);
int socket_connect(int sockfd, const sock_addr_t *addr);
int socket_send(int sockfd, const uint8_t *buf, uint32_t len, int flags);
int socket_recv(int sockfd, uint8_t *buf, uint32_t len, int flags);
int socket_close(int sockfd);
int socket_setopt(int sockfd, int level, int optname, const void *optval, int optlen);

/* Network interface functions */
void netif_register(netif_t *ni);
netif_t *netif_get(const char *name);

/* Socket table (extern, defined in socket.c) */
extern socket_t sockets[];
extern int socket_count;

/* Protocol input functions */
int icmp_input(const uint8_t *buf, uint32_t len, netif_t *ni);
int udp_input(const uint8_t *buf, uint32_t len, netif_t *ni);
int tcp_input(const uint8_t *buf, uint32_t len, netif_t *ni);

/* ARP functions */
int arp_resolve(netif_t *ni, const uint8_t *target_ip, uint8_t *target_mac);
int arp_handle_packet(const uint8_t *buf, uint32_t len, netif_t *ni);
void arp_cache_init(void);

/* IP functions */
int ip_output(netif_t *ni, const uint8_t *dest_ip, uint8_t protocol,
              const uint8_t *data, uint32_t data_len);
int ip_route(const uint8_t *dest_ip, netif_t **out_ni);
int ip_input(const uint8_t *buf, uint32_t len, netif_t *ni);

/* UDP functions */
int udp_send(netif_t *ni, const uint8_t *dest_ip, uint16_t src_port,
             uint16_t dest_port, const uint8_t *data, uint32_t data_len);

/* TCP internal functions */
int tcp_connect_internal(netif_t *ni, socket_t *sock);
int tcp_send_internal(netif_t *ni, socket_t *sock, const uint8_t *data, uint32_t len);
int tcp_close_internal(netif_t *ni, socket_t *sock);

/* ICMP functions */
void icmp_ping(netif_t *ni, const uint8_t *dest_ip, int count);

#endif
