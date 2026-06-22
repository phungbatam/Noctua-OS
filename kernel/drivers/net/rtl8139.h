#ifndef TVN_RTL8139_H
#define TVN_RTL8139_H

#include <stdint.h>

/* RTL8139 registers */
#define RTL8139_IDR0     0x00
#define RTL8139_IDR4     0x04
#define RTL8139_MAR0     0x08
#define RTL8139_MAR4     0x0C
#define RTL8139_TX_STATUS 0x10
#define RTL8139_TX_STATUS0 0x10
#define RTL8139_TX_STATUS1 0x14
#define RTL8139_TX_STATUS2 0x18
#define RTL8139_TX_STATUS3 0x1C
#define RTL8139_TX_ADDR0  0x20
#define RTL8139_TX_ADDR1  0x24
#define RTL8139_TX_ADDR2  0x28
#define RTL8139_TX_ADDR3  0x2C
#define RTL8139_RX_BUF    0x30
#define RTL8139_RX_EARLY  0x34
#define RTL8139_RX_EARLY_COUNT 0x36
#define RTL8139_TX_START  0x38
#define RTL8139_COMMAND   0x37
#define RTL8139_CAPR      0x38
#define RTL8139_CBR       0x3A
#define RTL8139_IMR       0x3C
#define RTL8139_ISR       0x3E
#define RTL8139_TCR       0x40
#define RTL8139_RCR       0x44
#define RTL8139_9346CR    0x50
#define RTL8139_CONFIG0   0x51
#define RTL8139_CONFIG1   0x52
#define RTL8139_MSR       0x58
#define RTL8139_BMCR      0x62

/* Commands */
#define RTL_CMD_RESET  0x10
#define RTL_CMD_RX_EN  0x08
#define RTL_CMD_TX_EN  0x04
#define RTL_CMD_RX_BUF_EMPTY 0x01

/* Interrupt status bits */
#define RTL_ISR_ROK    (1 << 0)
#define RTL_ISR_TOK    (1 << 2)
#define RTL_ISR_RX_ERR (1 << 4)
#define RTL_ISR_TX_ERR (1 << 6)
#define RTL_ISR_RX_OVW (1 << 5)
#define RTL_ISR_SERR   (1 << 15)

/* Receive config */
#define RTL_RCR_AAP   (1 << 0)
#define RTL_RCR_APM   (1 << 1)
#define RTL_RCR_AM    (1 << 2)
#define RTL_RCR_AB    (1 << 3)
#define RTL_RCR_WRAP  (1 << 7)
#define RTL_RCR_RXFTH_256 (3 << 8)
#define RTL_RCR_RXFTH_NONE (7 << 8)
#define RTL_RCR_RBLEN_8K  (0 << 11)
#define RTL_RCR_RBLEN_16K (1 << 11)
#define RTL_RCR_RBLEN_32K (2 << 11)
#define RTL_RCR_RBLEN_64K (3 << 11)

/* Transmit config */
#define RTL_TCR_IFG96 (3 << 24)
#define RTL_TCR_DMA   (7 << 8)
#define RTL_TCR_MXDMA (7 << 8)
#define RTL_TCR_HWVER (7 << 24)

/* RX packet header */
typedef struct {
    uint16_t config;
    uint16_t length;
    uint16_t rssi;
    uint8_t  reserved;
    uint8_t  status;
} __attribute__((packed)) rtl8139_rx_header_t;

/* Ethernet frame */
typedef struct {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t type;
    uint8_t  payload[];
} __attribute__((packed)) ethernet_frame_t;

/* ARP packet */
typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint8_t  spa[4];
    uint8_t  tha[6];
    uint8_t  tpa[4];
} __attribute__((packed)) arp_packet_t;

/* IPv4 header */
typedef struct {
    uint8_t  ver_ihl;
    uint8_t  dscp_ecn;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t header_checksum;
    uint8_t  src_ip[4];
    uint8_t  dest_ip[4];
} __attribute__((packed)) ipv4_header_t;

/* UDP header */
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

/* Ethernet types */
#define ETH_TYPE_IP  0x0800
#define ETH_TYPE_ARP 0x0806

/* ARP operations */
#define ARP_REQUEST 1
#define ARP_REPLY   2

/* IP protocols */
#define IP_PROTO_UDP 17
#define IP_PROTO_TCP 6

/* RTL8139 state */
typedef struct {
    uint16_t io_base;
    uint8_t  mac[6];
    uint8_t  ip[4];
    uint8_t  *rx_buffer;
    uint32_t rx_buffer_phys;
    uint32_t tx_buffer[4];
    uint32_t tx_buffer_phys[4];
    int tx_cur;
    int present;
} rtl8139_t;

extern rtl8139_t rtl8139_dev;

int  rtl8139_init(uint16_t io_base);
int  rtl8139_send_packet(const uint8_t *data, uint32_t len);
int  rtl8139_receive_packet(uint8_t *buffer, uint32_t *len);
void rtl8139_handle_irq(void);
void rtl8139_register_irq(int irq_num);

/* Network utility functions (also declared in net/net.h) */
uint16_t net_checksum(uint16_t *data, int len);

#endif
