#include "net.h"
#include "string.h"
#include "mm/heap.h"
#include "screen.h"

uint16_t net_checksum(uint16_t *data, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < (len + 1) / 2; i++) {
        sum += data[i];
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum & 0xFFFF;
}

uint16_t net_checksum_pseudo(const uint8_t *src_ip, const uint8_t *dest_ip,
                             uint8_t protocol, uint16_t tcp_len,
                             const uint8_t *data, int data_len) {
    uint32_t sum = 0;
    uint16_t temp;

    temp = (src_ip[0] << 8) | src_ip[1];
    sum += temp;
    temp = (src_ip[2] << 8) | src_ip[3];
    sum += temp;
    temp = (dest_ip[0] << 8) | dest_ip[1];
    sum += temp;
    temp = (dest_ip[2] << 8) | dest_ip[3];
    sum += temp;
    sum += protocol;
    sum += tcp_len;

    for (int i = 0; i < data_len; i += 2) {
        uint16_t word;
        if (i + 1 < data_len) {
            word = (data[i] << 8) | data[i + 1];
        } else {
            word = data[i] << 8;
        }
        sum += word;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum & 0xFFFF;
}

void net_ip_to_str(uint8_t *ip, char *str) {
    char buf[4][4];
    for (int i = 0; i < 4; i++) {
        int2str(ip[i], buf[i]);
    }
    strcpy(str, buf[0]);
    strcat(str, ".");
    strcat(str, buf[1]);
    strcat(str, ".");
    strcat(str, buf[2]);
    strcat(str, ".");
    strcat(str, buf[3]);
}

int net_str_to_ip(const char *str, uint8_t *ip) {
    int val = 0;
    int octet = 0;
    const char *p = str;

    while (*p) {
        if (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
        } else if (*p == '.') {
            if (octet >= 4 || val > 255) return -1;
            ip[octet++] = val;
            val = 0;
        } else {
            return -1;
        }
        p++;
    }
    if (octet != 3 || val > 255) return -1;
    ip[3] = val;
    return 0;
}

void net_mac_to_str(uint8_t *mac, char *str) {
    const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        str[i * 3] = hex[(mac[i] >> 4) & 0xF];
        str[i * 3 + 1] = hex[mac[i] & 0xF];
        str[i * 3 + 2] = (i < 5) ? ':' : '\0';
    }
}


