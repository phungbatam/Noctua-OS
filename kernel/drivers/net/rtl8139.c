#include "rtl8139.h"
#include "ports.h"
#include "string.h"
#include "mm/page.h"
#include "screen.h"
#include "isr.h"
#include "heap.h"
#include "net/net.h"

rtl8139_t rtl8139_dev;

#define RX_BUF_SIZE 8192

static void rtl8139_read_mac(uint16_t io_base) {
    uint32_t mac0 = inl(io_base + RTL8139_IDR0);
    uint32_t mac4 = inl(io_base + RTL8139_IDR4);
    rtl8139_dev.mac[0] = mac0 & 0xFF;
    rtl8139_dev.mac[1] = (mac0 >> 8) & 0xFF;
    rtl8139_dev.mac[2] = (mac0 >> 16) & 0xFF;
    rtl8139_dev.mac[3] = (mac0 >> 24) & 0xFF;
    rtl8139_dev.mac[4] = mac4 & 0xFF;
    rtl8139_dev.mac[5] = (mac4 >> 8) & 0xFF;
}

int rtl8139_init(uint16_t io_base) {
    memset(&rtl8139_dev, 0, sizeof(rtl8139_dev));
    rtl8139_dev.io_base = io_base;

    /* Power on the chip */
    outb(io_base + 0x52, 0x00);

    /* Reset */
    outb(io_base + RTL8139_COMMAND, RTL_CMD_RESET);
    while (inb(io_base + RTL8139_COMMAND) & RTL_CMD_RESET);

    /* Read MAC address */
    rtl8139_read_mac(io_base);

    /* Allocate RX buffer */
    rtl8139_dev.rx_buffer = (uint8_t *)pmem_alloc_page();
    if (!rtl8139_dev.rx_buffer) return -1;
    rtl8139_dev.rx_buffer_phys = (uint32_t)rtl8139_dev.rx_buffer;

    /* Set RX buffer address */
    outl(io_base + RTL8139_RX_BUF, rtl8139_dev.rx_buffer_phys);

    /* Allocate TX buffers */
    for (int i = 0; i < 4; i++) {
        rtl8139_dev.tx_buffer[i] = (uint32_t)pmem_alloc_page();
        if (!rtl8139_dev.tx_buffer[i]) return -1;
        rtl8139_dev.tx_buffer_phys[i] = rtl8139_dev.tx_buffer[i];
        outl(io_base + RTL8139_TX_ADDR0 + i * 4, rtl8139_dev.tx_buffer_phys[i]);
    }
    rtl8139_dev.tx_cur = 0;

    /* Enable RX/TX */
    outb(io_base + RTL8139_COMMAND, RTL_CMD_RX_EN | RTL_CMD_TX_EN);

    /* Configure receive: accept broadcast + physical match + multicast */
    outl(io_base + RTL8139_RCR, RTL_RCR_AAP | RTL_RCR_APM | RTL_RCR_AM | RTL_RCR_AB | RTL_RCR_RXFTH_256 | RTL_RCR_RBLEN_8K);

    /* Configure transmit */
    outl(io_base + RTL8139_TCR, RTL_TCR_IFG96 | RTL_TCR_DMA);

    /* Enable interrupts */
    outw(io_base + RTL8139_IMR, RTL_ISR_ROK | RTL_ISR_TOK | RTL_ISR_RX_ERR | RTL_ISR_TX_ERR | RTL_ISR_RX_OVW);

    rtl8139_dev.present = 1;

    char mac_str[18];
    net_mac_to_str(rtl8139_dev.mac, mac_str);
    screen_set_content_color(C_INFO);
    screen_term_write("RTL8139: Initialized - MAC ");
    screen_term_write(mac_str);
    screen_term_write("\n");

    return 0;
}

int rtl8139_send_packet(const uint8_t *data, uint32_t len) {
    if (!rtl8139_dev.present || !data || len == 0) return -1;

    int tx = rtl8139_dev.tx_cur;
    uint8_t *tx_buf = (uint8_t *)rtl8139_dev.tx_buffer[tx];

    memcpy(tx_buf, data, len);
    if (len < 60) {
        memset(tx_buf + len, 0, 60 - len);
        len = 60;
    }

    outl(rtl8139_dev.io_base + RTL8139_TX_STATUS0 + tx * 4, len);
    rtl8139_dev.tx_cur = (tx + 1) % 4;

    return 0;
}

int rtl8139_receive_packet(uint8_t *buffer, uint32_t *len) {
    if (!rtl8139_dev.present || !buffer || !len) return -1;

    uint16_t capr = inw(rtl8139_dev.io_base + RTL8139_CAPR);
    capr = (capr + 16) % RX_BUF_SIZE;

    while (1) {
        uint16_t rx_len = *(volatile uint16_t *)(rtl8139_dev.rx_buffer + capr + 2);
        (void)rx_len;

        if (rx_len == 0xFFF0) break;

        uint32_t pkt_len = rx_len - 4;
        uint32_t pkt_offset = capr + 4;

        if (pkt_offset + pkt_len > RX_BUF_SIZE) {
            uint32_t first_part = RX_BUF_SIZE - pkt_offset;
            memcpy(buffer, rtl8139_dev.rx_buffer + pkt_offset, first_part);
            memcpy(buffer + first_part, rtl8139_dev.rx_buffer, pkt_len - first_part);
        } else {
            memcpy(buffer, rtl8139_dev.rx_buffer + pkt_offset, pkt_len);
        }

        *len = pkt_len;

        capr = (capr + rx_len + 4 + 3) & ~3;
        capr %= RX_BUF_SIZE;
        outw(rtl8139_dev.io_base + RTL8139_CAPR, capr - 16);

        return 1;
    }

    /* No more packets */
    outb(rtl8139_dev.io_base + RTL8139_COMMAND, RTL_CMD_RX_BUF_EMPTY);
    return 0;
}

static int rtl8139_arp_send(uint8_t *target_ip, uint16_t oper) {
    uint8_t packet[42];
    memset(packet, 0, 42);

    ethernet_frame_t *eth = (ethernet_frame_t *)packet;
    arp_packet_t *arp = (arp_packet_t *)(packet + 14);

    memset(eth->dest_mac, 0xFF, 6);
    memcpy(eth->src_mac, rtl8139_dev.mac, 6);
    eth->type = 0x0608;
    arp->htype = 0x0100;
    arp->ptype = 0x0008;
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = oper;
    memcpy(arp->sha, rtl8139_dev.mac, 6);
    memcpy(arp->spa, rtl8139_dev.ip, 4);
    memcpy(arp->tpa, target_ip, 4);

    return rtl8139_send_packet(packet, 42);
}

static void __attribute__((unused)) rtl8139_send_udp(uint8_t *dest_ip, uint16_t dest_port,
                              uint16_t src_port, const uint8_t *data, uint32_t data_len) {
    uint32_t total_len = 14 + 20 + 8 + data_len;
    uint8_t *packet = (uint8_t *)kmalloc(total_len);
    if (!packet) return;
    memset(packet, 0, total_len);

    ethernet_frame_t *eth = (ethernet_frame_t *)packet;
    ipv4_header_t *ip = (ipv4_header_t *)(packet + 14);
    udp_header_t *udp = (udp_header_t *)(packet + 14 + 20);

    /* Ethernet */
    memset(eth->dest_mac, 0xFF, 6);
    memcpy(eth->src_mac, rtl8139_dev.mac, 6);
        eth->type = 0x0008;

    /* IP */
    ip->ver_ihl = 0x45;
    ip->total_length = (20 + 8 + data_len);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    memcpy(ip->src_ip, rtl8139_dev.ip, 4);
    memcpy(ip->dest_ip, dest_ip, 4);
    ip->header_checksum = net_checksum((uint16_t *)ip, 20);

    /* UDP */
    udp->src_port = src_port;
    udp->dest_port = dest_port;
    udp->length = 0x0800 + data_len;
    memcpy(packet + 14 + 20 + 8, data, data_len);

    rtl8139_send_packet(packet, total_len);
    kfree(packet);
}

void rtl8139_handle_irq(void) {
    if (!rtl8139_dev.present) return;

    uint16_t status = inw(rtl8139_dev.io_base + RTL8139_ISR);

    if (status & RTL_ISR_ROK) {
        uint8_t buf[1514];
        uint32_t len = 0;
        while (rtl8139_receive_packet(buf, &len)) {
            if (len < 14) continue;

            ethernet_frame_t *eth = (ethernet_frame_t *)buf;

            if (eth->type == 0x0608 && len >= 42) {
                arp_packet_t *arp = (arp_packet_t *)(buf + 14);
                if (arp->oper == 0x0100) {
                    rtl8139_arp_send(arp->spa, ARP_REPLY);
                }
            }
        }
    }

    /* Acknowledge interrupts */
    outw(rtl8139_dev.io_base + RTL8139_ISR, status);
}

static void rtl8139_irq_handler(struct registers *r) {
    (void)r;
    rtl8139_handle_irq();
}

void rtl8139_register_irq(int irq_num) {
    irq_register_handler(irq_num, rtl8139_irq_handler);
}
