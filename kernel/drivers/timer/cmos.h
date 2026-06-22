#ifndef TVN_CMOS_H
#define TVN_CMOS_H

#include <stdint.h>

#define CMOS_PORT_ADDR 0x70
#define CMOS_PORT_DATA 0x71

#define CMOS_REG_SEC         0x00
#define CMOS_REG_MIN         0x02
#define CMOS_REG_HOUR        0x04
#define CMOS_REG_DAY         0x07
#define CMOS_REG_MONTH       0x08
#define CMOS_REG_YEAR        0x09
#define CMOS_REG_STATUS_A    0x0A
#define CMOS_REG_STATUS_B    0x0B
#define CMOS_REG_STATUS_C    0x0C
#define CMOS_REG_STATUS_D    0x0D
#define CMOS_REG_DIAGNOSTIC  0x0E
#define CMOS_REG_SHUTDOWN    0x0F
#define CMOS_REG_DISK_DRIVE  0x12
#define CMOS_REG_EQUIPMENT   0x14
#define CMOS_REG_BASEMEM_L   0x15
#define CMOS_REG_BASEMEM_H   0x16
#define CMOS_REG_EXTMEM_L    0x17
#define CMOS_REG_EXTMEM_H    0x18
#define CMOS_REG_DISK1_TYPE  0x19
#define CMOS_REG_DISK2_TYPE  0x1A

#define CMOS_STATUS_A_UIP   0x80
#define CMOS_STATUS_B_24HR  0x02
#define CMOS_STATUS_B_BIN   0x04

uint8_t cmos_read(uint8_t reg);
void cmos_write(uint8_t reg, uint8_t value);
uint16_t cmos_read_word(uint8_t reg_low, uint8_t reg_high);
void cmos_wait(void);

uint16_t cmos_get_base_memory(void);
uint16_t cmos_get_ext_memory(void);
uint8_t  cmos_get_disk1_type(void);
uint8_t  cmos_get_disk2_type(void);
uint16_t cmos_get_equipment(void);
uint8_t  cmos_get_diagnostic(void);

int cmos_checksum_valid(void);
uint16_t cmos_checksum_compute(void);

#endif
