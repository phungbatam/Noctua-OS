#include "string.h"

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

void *memset(void *ptr, int value, size_t num) {
    unsigned char *p = (unsigned char *)ptr;
    for (size_t i = 0; i < num; i++) p[i] = (unsigned char)value;
    return ptr;
}

void *memcpy(void *dest, const void *src, size_t num) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < num; i++) d[i] = s[i];
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void int2str(int num, char *buf) {
    int i = 0, j = 0, neg = 0;
    char tmp[16];
    if (num == 0) { buf[0] = '0'; buf[1] = 0; return; }
    if (num < 0) { neg = 1; num = -num; }
    while (num > 0) { tmp[i++] = (num % 10) + '0'; num /= 10; }
    if (neg) tmp[i++] = '-';
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d = *src) != '\0') { d++; src++; }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        d[i] = src[i];
    }
    for (; i < n; i++) {
        d[i] = '\0';
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d != '\0') d++;
    while ((*d = *src) != '\0') { d++; src++; }
    return dest;
}
