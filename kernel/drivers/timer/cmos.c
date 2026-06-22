#include "cmos.h"
#include "ports.h"

uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_PORT_ADDR, reg);
    return inb(CMOS_PORT_DATA);
}

void cmos_write(uint8_t reg, uint8_t value) {
    outb(CMOS_PORT_ADDR, reg);
    outb(CMOS_PORT_DATA, value);
}

void cmos_wait(void) {
    while (cmos_read(CMOS_REG_STATUS_A) & CMOS_STATUS_A_UIP);
}

uint16_t cmos_read_word(uint8_t reg_low, uint8_t reg_high) {
    return (uint16_t)cmos_read(reg_low) | ((uint16_t)cmos_read(reg_high) << 8);
}

uint16_t cmos_get_base_memory(void) {
    return cmos_read_word(CMOS_REG_BASEMEM_L, CMOS_REG_BASEMEM_H);
}

uint16_t cmos_get_ext_memory(void) {
    return cmos_read_word(CMOS_REG_EXTMEM_L, CMOS_REG_EXTMEM_H);
}

uint8_t cmos_get_disk1_type(void) {
    return cmos_read(CMOS_REG_DISK1_TYPE);
}

uint8_t cmos_get_disk2_type(void) {
    return cmos_read(CMOS_REG_DISK2_TYPE);
}

uint16_t cmos_get_equipment(void) {
    return cmos_read_word(CMOS_REG_EQUIPMENT, CMOS_REG_EQUIPMENT + 1);
}

uint8_t cmos_get_diagnostic(void) {
    return cmos_read(CMOS_REG_DIAGNOSTIC);
}

#define CMOS_CHECKSUM_START 0x10
#define CMOS_CHECKSUM_END   0x2D
#define CMOS_CHECKSUM_LOW   0x2E
#define CMOS_CHECKSUM_HIGH  0x2F

int cmos_checksum_valid(void) {
    uint16_t computed = cmos_checksum_compute();
    uint16_t stored = cmos_read_word(CMOS_CHECKSUM_LOW, CMOS_CHECKSUM_HIGH);
    return computed == stored;
}

uint16_t cmos_checksum_compute(void) {
    uint16_t sum = 0;
    for (uint8_t reg = CMOS_CHECKSUM_START; reg <= CMOS_CHECKSUM_END; reg++) {
        sum += cmos_read(reg);
    }
    return sum;
}
